/* AudioScope by BerryStudio 0.4-alpha — repair-specific measurements. */
#ifndef AS_CORE_REPAIR_H
#define AS_CORE_REPAIR_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    double wow_pct;
    double flutter_pct;
    double speed_ppm;
    double freq_hz;
    int periods;
} AsWowResult;

/* Wow & flutter from a nominally 3 kHz test tone. */
int as_wow_measure(const float *x, int n, double fs, AsWowResult *out);

/* Crosstalk in dB: rms(b) / rms(a). a is the driven channel. */
int as_crosstalk_db(const float *a, const float *b, int n, double *db);

/* Series-resistor impedance: Z = R * Vdut / (Vsource - Vdut). */
int as_impedance(double r_series, double v_dut, double v_source, double *z);

#ifdef __cplusplus
}
#endif

#endif
