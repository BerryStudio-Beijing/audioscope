/* AudioScope by BerryStudio 0.5-alpha — input mode presets. */
#include "core/api.h"

#include <ctype.h>
#include <string.h>

static const AsInputModeSpec kModes[AS_MODE_COUNT] = {
    /* id           label                 nominal_dbu  max_vpp  Z(ohm)  phantom  frontend */
    {"line-pro",    "Line Pro (+4 dBu)",   4.0,        34.8,   10000.0, 0, 0},
    {"line-consumer","Line Consumer (-10 dBV)", -7.78,  8.94,   10000.0, 0, 0},
    {"di",          "DI (balanced line)",  4.0,        34.8,   10000.0, 0, 0},
    {"inst",        "Instrument (-20 dBV)",-17.78,     8.94,   1000000.0, 0, 0},
    {"mic",         "Mic (-60 dBu)",      -60.0,       0.22,   1500.0,  1, 0},
    {"probe",       "Probe 10:1",          0.0,        100.0,  10000000.0, 0, 1},
};

const AsInputModeSpec *as_input_mode_spec(AsInputMode mode) {
    if (mode < 0 || mode >= AS_MODE_COUNT) return NULL;
    return &kModes[mode];
}

const char *as_input_mode_id(AsInputMode mode) {
    const AsInputModeSpec *spec = as_input_mode_spec(mode);
    return spec ? spec->id : "?";
}

static int equals_ci(const char *a, const char *b) {
    if (!a || !b) return 0;
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        ++a; ++b;
    }
    return *a == '\0' && *b == '\0';
}

int as_input_mode_parse(const char *id) {
    if (!id || !id[0]) return -1;
    if (equals_ci(id, "line") || equals_ci(id, "line-pro")) return AS_MODE_LINE_PRO;
    if (equals_ci(id, "consumer") || equals_ci(id, "line-consumer")) return AS_MODE_LINE_CONSUMER;
    if (equals_ci(id, "di")) return AS_MODE_DI;
    if (equals_ci(id, "inst") || equals_ci(id, "instrument")) return AS_MODE_INST;
    if (equals_ci(id, "mic")) return AS_MODE_MIC;
    if (equals_ci(id, "probe")) return AS_MODE_PROBE;
    return -1;
}
