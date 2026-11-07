/* AudioScope by BerryStudio 0.6-alpha — phase, gain and drift measurement. */
#ifndef AS_CORE_PHASE_H
#define AS_CORE_PHASE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    double freq_hz;
    double phase_deg;
    double gain_db;
    double coherence; /* 0..1, reserved for later */
} AsPhaseResult;

/* Measures the phase/gain of b relative to a at a's dominant frequency. */
int as_phase_measure(const float *a, const float *b, int n, double fs,
                     AsPhaseResult *out);

/* Estimates relative clock drift (ppm) from phase slope over blocks.
 * Same-device channels share a clock; the caller must only use this across
 * two devices or in simulation. */
int as_drift_estimate(const float *a, const float *b, int n, double fs,
                      int block, double *ppm, double *freq_hz);

#ifdef __cplusplus
}
#endif

#endif
