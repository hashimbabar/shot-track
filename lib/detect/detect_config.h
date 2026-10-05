/*
 * detect_config.h - every tunable number for shot detection, in one place.
 *
 * IMPORTANT: these are starting guesses. They have NOT been tuned on real
 * recordings yet (the hardware hasn't arrived). The synthetic sessions in
 * tools/ were generated with these values in mind, so passing on them only
 * shows the code works as designed, not that the thresholds are right for
 * a real wrist. Retune from real data in v1 step 6.
 */
#ifndef DETECT_CONFIG_H
#define DETECT_CONFIG_H

/* Rate the IMU is sampled at. Used to turn the filter cutoff into a
 * filter coefficient. */
#define DETECT_SAMPLE_RATE_HZ       200.0f

/* Low-pass filter cutoff for the gyro and accel magnitudes. A one-pole
 * filter at 15 Hz keeps the ~10 Hz flick shape but smooths sensor noise.
 * It delays the signal by about 1 / (2 * pi * 15 Hz) = 10.6 ms. */
#define DETECT_LPF_CUTOFF_HZ        15.0f

/* A shot candidate starts when the filtered gyro magnitude rises above
 * this. The wrist snap at release is the fastest rotation in a shot. */
#define DETECT_GYRO_THRESHOLD_DPS   700.0f

/* The candidate ends when the gyro drops below THRESHOLD * RELEASE_RATIO.
 * Ending lower than the start (hysteresis) stops noise around the
 * threshold from splitting one peak into several. */
#define DETECT_RELEASE_RATIO        0.5f

/* The filtered accel magnitude must reach this somewhere inside the
 * candidate. Rejects a fast wrist turn with no real arm motion. 1 g is
 * just gravity at rest. */
#define DETECT_MIN_ACCEL_G          1.8f

/* After a shot is counted, ignore new peaks for this long. The
 * follow-through after a release can produce a second gyro peak; this
 * stops it being counted as another shot. Nobody shoots twice in 0.7 s. */
#define DETECT_REFRACTORY_MS        700u

#endif /* DETECT_CONFIG_H */
