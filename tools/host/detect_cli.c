/*
 * detect_cli.c - run lib/detect over a session CSV on the PC.
 *
 * Reads a log in the firmware's format (seq,t_ms,ax,ay,az,gx,gy,gz in raw
 * MPU-6050 counts; extra columns are ignored, lines starting with '#' are
 * skipped) and prints one line per detected shot:
 *
 *   shot,<t_ms>,<peak_gyro_dps>,<peak_accel_mg>
 *   count,<n>
 *
 * It's the same detect.c the firmware runs, compiled for the PC, so a
 * recorded session gives the same answer here as on the board.
 *
 * Build: cc -O2 -Ilib/detect lib/detect/detect.c tools/host/detect_cli.c -lm
 * (tools/replay.py does this for you.)
 */
#include <stdio.h>
#include <stdlib.h>
#include "detect.h"

/* Must match the firmware's IMU settings (task_sampling.c). */
#define ACCEL_LSB_PER_G   2048.0f   /* +-16 g */
#define GYRO_LSB_PER_DPS  16.4f     /* +-2000 dps */

int main(int argc, char **argv)
{
    FILE *f;
    char line[256];
    detect_t det;
    unsigned long line_no = 0, rows = 0;

    if (argc != 2) {
        fprintf(stderr, "usage: %s session.csv\n", argv[0]);
        return 2;
    }
    f = fopen(argv[1], "r");
    if (f == NULL) {
        perror(argv[1]);
        return 2;
    }

    detect_init(&det);
    while (fgets(line, sizeof(line), f) != NULL) {
        unsigned long seq, t_ms;
        int ax, ay, az, gx, gy, gz;
        detect_sample_t s;
        detect_shot_t shot;

        line_no++;
        if (line[0] == '#' || line[0] == 's') { /* comment or "seq,..." header */
            continue;
        }
        if (sscanf(line, "%lu,%lu,%d,%d,%d,%d,%d,%d",
                   &seq, &t_ms, &ax, &ay, &az, &gx, &gy, &gz) != 8) {
            fprintf(stderr, "%s:%lu: can't parse line\n", argv[1], line_no);
            fclose(f);
            return 1;
        }
        (void)seq;
        rows++;

        s.t_ms = (uint32_t)t_ms;
        s.ax_g = (float)ax / ACCEL_LSB_PER_G;
        s.ay_g = (float)ay / ACCEL_LSB_PER_G;
        s.az_g = (float)az / ACCEL_LSB_PER_G;
        s.gx_dps = (float)gx / GYRO_LSB_PER_DPS;
        s.gy_dps = (float)gy / GYRO_LSB_PER_DPS;
        s.gz_dps = (float)gz / GYRO_LSB_PER_DPS;

        if (detect_update(&det, &s, &shot)) {
            printf("shot,%lu,%.0f,%.0f\n", (unsigned long)shot.t_ms,
                   shot.peak_gyro_dps, shot.peak_accel_g * 1000.0f);
        }
    }
    fclose(f);

    printf("count,%lu\n", (unsigned long)detect_shot_count(&det));
    fprintf(stderr, "%lu samples, %lu suppressed by refractory, %lu rejected by accel check\n",
            rows, (unsigned long)det.suppressed, (unsigned long)det.rejected_accel);
    return 0;
}
