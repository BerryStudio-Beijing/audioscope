/* Small shared min/max/clamp helpers for the native UI toolkit. */
#ifndef AS_UI_NATIVE_COMPAT_H
#define AS_UI_NATIVE_COMPAT_H

#define AS_MIN(a, b) ((a) < (b) ? (a) : (b))
#define AS_MAX(a, b) ((a) > (b) ? (a) : (b))
#define AS_CLAMP(v, lo, hi) (AS_MIN(AS_MAX((v), (lo)), (hi)))

#endif
