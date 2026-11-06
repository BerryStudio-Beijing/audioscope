/* AudioScope by BerryStudio 0.2-alpha — ring buffer regression test (C11). */
#include "core/ring.h"

#include <stdio.h>

int main(void) {
    AsRing ring;
    if (as_ring_init(&ring, 1024, 2) != 0) {
        fprintf(stderr, "ring init failed\n");
        return 1;
    }
    float in[200];
    float out[200];
    for (int i = 0; i < 100; ++i) {
        in[2 * i] = (float)i;
        in[2 * i + 1] = -(float)i;
    }
    as_ring_write(&ring, in, 100);
    int got = as_ring_read_latest(&ring, out, 100);
    int ok = got == 100 && out[0] == 0.0f && out[198] == 99.0f &&
             out[199] == -99.0f;
    as_ring_write(&ring, in, 100);
    got = as_ring_read_latest(&ring, out, 50);
    ok = ok && got == 50 && out[0] == 50.0f && out[98] == 99.0f;
    as_ring_free(&ring);
    if (!ok) {
        fprintf(stderr, "ring test failed\n");
        return 1;
    }
    printf("ring ok\n");
    return 0;
}
