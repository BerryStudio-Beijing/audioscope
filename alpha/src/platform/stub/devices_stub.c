/* AudioScope by BerryStudio 0.3-alpha — device enumeration stub. */
#include "core/api.h"

int as_platform_list_devices(AsDeviceInfo *out, int max) {
    (void)out; (void)max;
    return 0;
}

const char *as_platform_name(void) {
    return "stub";
}
