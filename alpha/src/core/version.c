/* AudioScope 0.1-alpha — version and status strings. */
#include "core/api.h"

const char *as_brand_string(void) {
    return AS_BRAND;
}

const char *as_version_string(void) {
    return AS_ALPHA_VERSION;
}

const char *as_status_string(AsStatus status) {
    switch (status) {
    case AS_OK: return "ok";
    case AS_ERR_INVALID: return "invalid";
    case AS_ERR_STATE: return "wrong state";
    case AS_ERR_IO: return "io error";
    case AS_ERR_NOT_IMPLEMENTED: return "not implemented";
    case AS_ERR_RANGE: return "out of range";
    default: return "unknown";
    }
}
