/* AudioScope by BerryStudio 0.7-alpha — unit conversions.
 *
 * dBFS is the only native fact. Voltage requires a calibration full-scale
 * value and, for Vpp, an explicit crest factor assumption.
 */
#include "core/api.h"

#include <math.h>

const char *as_unit_name(AsUnit unit) {
    switch (unit) {
    case AS_UNIT_DBFS: return "dBFS";
    case AS_UNIT_DBU: return "dBu";
    case AS_UNIT_DBV: return "dBV";
    case AS_UNIT_VRMS: return "Vrms";
    case AS_UNIT_VPP: return "Vpp";
    default: return "?";
    }
}

static int unit_valid(AsUnit u) {
    return u >= AS_UNIT_DBFS && u <= AS_UNIT_VPP;
}

AsStatus as_units_convert(double value, AsUnit from, AsUnit to,
                          double fs_volts_rms, double crest, double *out) {
    if (!out || !unit_valid(from) || !unit_valid(to)) return AS_ERR_INVALID;
    if (!(fs_volts_rms > 0.0) || !isfinite(fs_volts_rms)) return AS_ERR_INVALID;
    if (!(crest > 0.0) || !isfinite(crest)) return AS_ERR_INVALID;
    if (!isfinite(value)) return AS_ERR_INVALID;

    /* Normalise to Vrms first. */
    double vrms = 0.0;
    switch (from) {
    case AS_UNIT_DBFS:
        vrms = fs_volts_rms * pow(10.0, value / 20.0);
        break;
    case AS_UNIT_DBU:
        vrms = 0.775 * pow(10.0, value / 20.0);
        break;
    case AS_UNIT_DBV:
        vrms = 1.0 * pow(10.0, value / 20.0);
        break;
    case AS_UNIT_VRMS:
        vrms = value;
        break;
    case AS_UNIT_VPP:
        vrms = value / (2.0 * crest);
        break;
    default:
        return AS_ERR_INVALID;
    }

    if (!(vrms >= 0.0) || !isfinite(vrms)) return AS_ERR_INVALID;

    switch (to) {
    case AS_UNIT_DBFS:
        if (vrms <= 0.0) return AS_ERR_RANGE;
        *out = 20.0 * log10(vrms / fs_volts_rms);
        break;
    case AS_UNIT_DBU:
        if (vrms <= 0.0) return AS_ERR_RANGE;
        *out = 20.0 * log10(vrms / 0.775);
        break;
    case AS_UNIT_DBV:
        if (vrms <= 0.0) return AS_ERR_RANGE;
        *out = 20.0 * log10(vrms / 1.0);
        break;
    case AS_UNIT_VRMS:
        *out = vrms;
        break;
    case AS_UNIT_VPP:
        *out = 2.0 * crest * vrms;
        break;
    default:
        return AS_ERR_INVALID;
    }
    return AS_OK;
}
