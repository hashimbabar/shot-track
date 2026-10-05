/*
 * test_detect.c - unit tests for lib/detect, run on the PC with
 * `pio test -e native`.
 *
 * All signals here are synthetic: a "flick" is a Gaussian bump in gyro
 * rate plus a smaller bump in acceleration, on top of 1 g of gravity.
 * They check the logic (threshold, accel check, refractory window,
 * filter start-up), not how well the thresholds suit a real wrist.
 */
#include <math.h>
#include <unity.h>
#include "detect.h"
#include "detect_config.h"

#define FS_HZ        200
#define DT_MS        (1000 / FS_HZ)
#define MAX_PULSES   32

typedef struct {
    uint32_t t_ms;     /* centre of the bump */
    float gyro_dps;    /* gyro bump height */
    float accel_g;     /* accel bump height, added to gravity */
} pulse_t;

static detect_t det;
static detect_shot_t shots[MAX_PULSES];
static int n_shots;

void setUp(void)
{
    detect_init(&det);
    n_shots = 0;
}

void tearDown(void)
{
}

static float bump(float t, float centre, float sigma_ms)
{
    const float x = (t - centre) / sigma_ms;
    return expf(-0.5f * x * x);
}

/* Feed duration_ms of signal containing the given pulses through the
 * detector. Rotation is around the x axis; gravity is on z. */
static void run(const pulse_t *p, int n, uint32_t start_ms, uint32_t duration_ms)
{
    for (uint32_t t = 0; t < duration_ms; t += DT_MS) {
        detect_sample_t s = {0};
        detect_shot_t shot;
        const uint32_t now = start_ms + t;

        s.t_ms = now;
        s.az_g = 1.0f;
        for (int i = 0; i < n; i++) {
            /* Signed difference so start times near the 32-bit wrap work. */
            const float rel = (float)(int32_t)(now - p[i].t_ms);
            s.gx_dps += p[i].gyro_dps * bump(rel, 0.0f, 25.0f);
            s.ay_g += p[i].accel_g * bump(rel, 0.0f, 30.0f);
        }
        if (detect_update(&det, &s, &shot) && n_shots < MAX_PULSES) {
            shots[n_shots++] = shot;
        }
    }
}

void test_flat_signal_gives_no_shots(void)
{
    run(NULL, 0, 0, 10000);
    TEST_ASSERT_EQUAL_UINT32(0, detect_shot_count(&det));
}

void test_single_flick_is_one_shot_at_the_right_time(void)
{
    const pulse_t p[] = {{1000, 1200.0f, 2.5f}};

    run(p, 1, 0, 3000);
    TEST_ASSERT_EQUAL_UINT32(1, detect_shot_count(&det));
    TEST_ASSERT_EQUAL_INT(1, n_shots);
    /* The filter delays the peak by roughly 10 ms; allow 30 ms. */
    TEST_ASSERT_UINT32_WITHIN(30, 1000, shots[0].t_ms);
    TEST_ASSERT_TRUE(shots[0].peak_gyro_dps > DETECT_GYRO_THRESHOLD_DPS);
}

void test_two_peaks_inside_refractory_window_count_once(void)
{
    /* A release followed 200 ms later by a follow-through bump. */
    const pulse_t p[] = {{1000, 1200.0f, 2.5f}, {1200, 900.0f, 2.0f}};

    run(p, 2, 0, 3000);
    TEST_ASSERT_EQUAL_UINT32(1, detect_shot_count(&det));
    TEST_ASSERT_EQUAL_UINT32(1, det.suppressed);
}

void test_two_shots_far_apart_count_twice(void)
{
    const pulse_t p[] = {{1000, 1200.0f, 2.5f}, {2500, 1200.0f, 2.5f}};

    run(p, 2, 0, 4000);
    TEST_ASSERT_EQUAL_UINT32(2, detect_shot_count(&det));
    TEST_ASSERT_EQUAL_UINT32(0, det.suppressed);
}

void test_flick_below_threshold_is_ignored(void)
{
    const pulse_t p[] = {{1000, 500.0f, 2.5f}};

    run(p, 1, 0, 3000);
    TEST_ASSERT_EQUAL_UINT32(0, detect_shot_count(&det));
}

void test_fast_rotation_without_acceleration_is_rejected(void)
{
    const pulse_t p[] = {{1000, 1200.0f, 0.0f}};

    run(p, 1, 0, 3000);
    TEST_ASSERT_EQUAL_UINT32(0, detect_shot_count(&det));
    TEST_ASSERT_EQUAL_UINT32(1, det.rejected_accel);
}

void test_dribbling_gives_no_shots(void)
{
    /* 10 s of dribbling: a 400 dps wrist bump every 500 ms. */
    pulse_t p[20];
    for (int i = 0; i < 20; i++) {
        p[i].t_ms = 250u + 500u * (uint32_t)i;
        p[i].gyro_dps = 400.0f;
        p[i].accel_g = 1.5f;
    }

    run(p, 20, 0, 10500);
    TEST_ASSERT_EQUAL_UINT32(0, detect_shot_count(&det));
}

void test_refractory_window_survives_timer_wraparound(void)
{
    /* Two peaks 200 ms apart, straddling the point where the 32-bit
     * millisecond counter wraps back to 0. */
    const uint32_t start = 0xFFFFFFFFu - 1500u;
    const pulse_t p[] = {{start + 1400u, 1200.0f, 2.5f}, {start + 1600u, 1200.0f, 2.5f}};

    run(p, 2, start, 3000);
    TEST_ASSERT_EQUAL_UINT32(1, detect_shot_count(&det));
}

void test_filter_starts_at_first_sample(void)
{
    detect_sample_t s = {.t_ms = 0, .az_g = 1.0f};

    detect_update(&det, &s, NULL);
    /* No ramp up from zero: the filtered accel is already 1 g. */
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, det.accel_f);
}

void test_lpf_alpha_matches_formula(void)
{
    /* 1 - exp(-2*pi*15/200) = 0.3757 */
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.3757f, detect_lpf_alpha(15.0f, 200.0f));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_flat_signal_gives_no_shots);
    RUN_TEST(test_single_flick_is_one_shot_at_the_right_time);
    RUN_TEST(test_two_peaks_inside_refractory_window_count_once);
    RUN_TEST(test_two_shots_far_apart_count_twice);
    RUN_TEST(test_flick_below_threshold_is_ignored);
    RUN_TEST(test_fast_rotation_without_acceleration_is_rejected);
    RUN_TEST(test_dribbling_gives_no_shots);
    RUN_TEST(test_refractory_window_survives_timer_wraparound);
    RUN_TEST(test_filter_starts_at_first_sample);
    RUN_TEST(test_lpf_alpha_matches_formula);
    return UNITY_END();
}
