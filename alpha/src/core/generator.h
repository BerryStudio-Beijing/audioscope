/* AudioScope by BerryStudio 0.6-alpha — signal generator (mono float). */
#ifndef AS_CORE_GENERATOR_H
#define AS_CORE_GENERATOR_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    double fs;
    double phase;
} AsGenerator;

void as_generator_init(AsGenerator *gen, double fs);
void as_generator_sine(AsGenerator *gen, double freq, double amp,
                       float *out, int frames);
void as_generator_square(AsGenerator *gen, double freq, double amp,
                         float *out, int frames);

#ifdef __cplusplus
}
#endif

#endif
