/* AudioScope 0.1-alpha — analysis windows. */
#ifndef AS_CORE_WINDOW_H
#define AS_CORE_WINDOW_H

#ifdef __cplusplus
extern "C" {
#endif

void as_window_hann(double *w, int n);
void as_window_blackman(double *w, int n);
void as_window_rect(double *w, int n);

#ifdef __cplusplus
}
#endif

#endif
