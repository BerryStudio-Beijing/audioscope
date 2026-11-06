/* AudioScope by BerryStudio 0.5-alpha — THD/IMD measurement. */
#ifndef AS_CORE_TONE_H
#define AS_CORE_TONE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    double freq_hz;
    double fundamental;
    double thd_pct;
    double thd_db;
    double thdn_pct;
    double thdn_db;
    double snr_db;
    double sinad_db;
    double sfdr_db;
    double enob;
} AsToneResult;

/* THD/THD+N/SNR/SINAD/SFDR from a windowed FFT around the strongest tone. */
int as_tone_analyze(const float *x, int n, double fs, AsToneResult *out);

/* SMPTE: 60 Hz + 7 kHz (4:1). DIN: 250 Hz + 8 kHz (4:1). */
int as_imd_analyze(const float *x, int n, double fs, int din,
                   double *imd_pct, double *f1_out, double *f2_out);

#ifdef __cplusplus
}
#endif

#endif
