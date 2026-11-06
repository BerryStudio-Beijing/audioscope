/* AudioScope by BerryStudio 0.5-alpha — time-domain statistics. */
#ifndef AS_CORE_STATS_H
#define AS_CORE_STATS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    double peak;
    double vpp;
    double rms;
    double mean;
    double freq;
    int zero_crossings;
} AsStats;

void as_stats_compute(const float *x, int n, double fs, AsStats *out);
double as_linear_to_db(double amp);

#ifdef __cplusplus
}
#endif

#endif
