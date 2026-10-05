/*
 * detect.h - basketball shot detection from wrist IMU samples.
 *
 * Pure C: no HAL, no RTOS, no malloc. The same file runs in the
 * firmware's detection task, in the PC unit tests, and in
 * tools/host/detect_cli.c for replaying recorded sessions.
 *
 * How it works, per sample:
 *   1. Take the magnitude of the gyro vector and of the accel vector,
 *      so the result doesn't depend on how the sensor sits on the wrist.
 *   2. Smooth both with a one-pole low-pass filter.
 *   3. When the gyro magnitude goes above the threshold, start tracking
 *      a peak. When it falls back below the release level, the peak is
 *      over: it's a shot if the accel also went high enough.
 *   4. Drop any shot that comes within the refractory window of the last
 *      one, so a single shot can't be counted twice.
 */
#ifndef DETECT_H
#define DETECT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* One IMU sample in physical units. */
typedef struct {
    uint32_t t_ms;              /* sample time, used for the refractory window */
    float ax_g, ay_g, az_g;     /* acceleration in g */
    float gx_dps, gy_dps, gz_dps; /* angular rate in degrees per second */
} detect_sample_t;

/* What gets reported for each detected shot. */
typedef struct {
    uint32_t t_ms;              /* time of the gyro peak */
    float peak_gyro_dps;        /* filtered gyro magnitude at the peak */
    float peak_accel_g;         /* largest filtered accel magnitude in the candidate */
} detect_shot_t;

typedef struct {
    float alpha;                /* low-pass filter coefficient */
    bool primed;                /* false until the first sample seeds the filters */
    float gyro_f;               /* filtered gyro magnitude */
    float accel_f;              /* filtered accel magnitude */

    bool in_peak;               /* currently above threshold? */
    detect_shot_t cand;         /* peak being tracked */

    bool have_last;             /* has any shot been counted yet? */
    uint32_t last_shot_ms;

    uint32_t shots;             /* shots counted */
    uint32_t suppressed;        /* peaks dropped by the refractory window */
    uint32_t rejected_accel;    /* peaks dropped by the accel check */
} detect_t;

void detect_init(detect_t *d);

/*
 * Feed one sample. Returns true when a shot has just been confirmed, and
 * fills *shot (if not NULL). A shot is confirmed when its peak is over,
 * so it is reported a little after the peak itself.
 */
bool detect_update(detect_t *d, const detect_sample_t *s, detect_shot_t *shot);

uint32_t detect_shot_count(const detect_t *d);

/* Filter coefficient for a one-pole low-pass at fc Hz, sampled at fs Hz. */
float detect_lpf_alpha(float fc_hz, float fs_hz);

#ifdef __cplusplus
}
#endif

#endif /* DETECT_H */
