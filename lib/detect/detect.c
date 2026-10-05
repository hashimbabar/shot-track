/*
 * detect.c - shot detection. See detect.h for the overview and
 * detect_config.h for the numbers.
 */
#include <math.h>
#include <stddef.h>
#include <string.h>
#include "detect.h"
#include "detect_config.h"

#define PI_F 3.14159265f

/*
 * One-pole low-pass ("exponential moving average"):
 *   y[n] = y[n-1] + alpha * (x[n] - y[n-1])
 * Choosing alpha = 1 - exp(-2*pi*fc/fs) gives a -3 dB cutoff near fc.
 * Chosen over a longer FIR filter because it needs one multiply and one
 * float of state per signal, and its delay is small at 15 Hz.
 */
float detect_lpf_alpha(float fc_hz, float fs_hz)
{
    return 1.0f - expf(-2.0f * PI_F * fc_hz / fs_hz);
}

void detect_init(detect_t *d)
{
    memset(d, 0, sizeof(*d));
    d->alpha = detect_lpf_alpha(DETECT_LPF_CUTOFF_HZ, DETECT_SAMPLE_RATE_HZ);
}

static float magnitude(float x, float y, float z)
{
    return sqrtf(x * x + y * y + z * z);
}

/* Called when a candidate peak ends. Decides whether it counts. */
static bool finish_candidate(detect_t *d, detect_shot_t *shot)
{
    const detect_shot_t *c = &d->cand;

    if (c->peak_accel_g < DETECT_MIN_ACCEL_G) {
        d->rejected_accel++;
        return false;
    }

    /* Unsigned subtraction still gives the right gap when the
     * millisecond counter wraps around (after ~49 days). */
    if (d->have_last && (uint32_t)(c->t_ms - d->last_shot_ms) < DETECT_REFRACTORY_MS) {
        d->suppressed++;
        return false;
    }

    d->have_last = true;
    d->last_shot_ms = c->t_ms;
    d->shots++;
    if (shot != NULL) {
        *shot = *c;
    }
    return true;
}

bool detect_update(detect_t *d, const detect_sample_t *s, detect_shot_t *shot)
{
    const float g = magnitude(s->gx_dps, s->gy_dps, s->gz_dps);
    const float a = magnitude(s->ax_g, s->ay_g, s->az_g);

    /* Start the filters at the first sample's value. Starting from zero
     * would make the filtered accel ramp up from 0 to 1 g at power-on. */
    if (!d->primed) {
        d->gyro_f = g;
        d->accel_f = a;
        d->primed = true;
    } else {
        d->gyro_f += d->alpha * (g - d->gyro_f);
        d->accel_f += d->alpha * (a - d->accel_f);
    }

    if (!d->in_peak) {
        if (d->gyro_f > DETECT_GYRO_THRESHOLD_DPS) {
            d->in_peak = true;
            d->cand.t_ms = s->t_ms;
            d->cand.peak_gyro_dps = d->gyro_f;
            d->cand.peak_accel_g = d->accel_f;
        }
        return false;
    }

    /* Inside a candidate: remember the highest point of each signal. */
    if (d->gyro_f > d->cand.peak_gyro_dps) {
        d->cand.peak_gyro_dps = d->gyro_f;
        d->cand.t_ms = s->t_ms;
    }
    if (d->accel_f > d->cand.peak_accel_g) {
        d->cand.peak_accel_g = d->accel_f;
    }

    if (d->gyro_f < DETECT_GYRO_THRESHOLD_DPS * DETECT_RELEASE_RATIO) {
        d->in_peak = false;
        return finish_candidate(d, shot);
    }
    return false;
}

uint32_t detect_shot_count(const detect_t *d)
{
    return d->shots;
}
