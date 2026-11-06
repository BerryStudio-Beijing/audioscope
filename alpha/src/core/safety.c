/* AudioScope 0.1-alpha — SafetyGuard state machine.
 *
 * The software can only gate, latch and warn. It cannot protect the sound card
 * from an over-voltage event; that is the job of the analog front end.
 */
#include "core/api.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct AsSafetyGuard {
    AsSafetyState state;
    int input_mode;
    double estimated_vpp;
    int frontend_present;
    int clip_channel;
    double clip_peak;
    char reason[192];
};

static void set_reason(AsSafetyGuard *g, const char *text) {
    if (!g) return;
    snprintf(g->reason, sizeof(g->reason), "%s", text ? text : "");
}

AsSafetyGuard *as_safety_create(void) {
    AsSafetyGuard *g = (AsSafetyGuard *)calloc(1, sizeof(*g));
    if (!g) return NULL;
    g->state = AS_SAFETY_IDLE;
    g->input_mode = 0;
    g->estimated_vpp = 0.0;
    g->frontend_present = 0;
    g->clip_channel = -1;
    g->clip_peak = 0.0;
    set_reason(g, "ready");
    return g;
}

void as_safety_destroy(AsSafetyGuard *guard) {
    free(guard);
}

AsStatus as_safety_precheck(AsSafetyGuard *g, int input_mode,
                            double estimated_vpp, int frontend_present,
                            char *reason, size_t reason_len) {
    if (!g) return AS_ERR_INVALID;
    if (g->state != AS_SAFETY_IDLE) {
        set_reason(g, "precheck requires IDLE (reset first)");
        if (reason && reason_len) snprintf(reason, reason_len, "%s", g->reason);
        return AS_ERR_STATE;
    }
    if (input_mode < 0 || input_mode > 5) {
        set_reason(g, "unknown input mode");
        if (reason && reason_len) snprintf(reason, reason_len, "%s", g->reason);
        return AS_ERR_INVALID;
    }
    if (!(estimated_vpp >= 0.0) || estimated_vpp > 100.0) {
        set_reason(g, "estimated voltage exceeds 100 Vpp");
        if (reason && reason_len) snprintf(reason, reason_len, "%s", g->reason);
        return AS_ERR_RANGE;
    }
    if (!frontend_present && estimated_vpp > 2.0) {
        set_reason(g, "no front end: estimated input above 2 Vpp");
        if (reason && reason_len) snprintf(reason, reason_len, "%s", g->reason);
        return AS_ERR_RANGE;
    }
    if (!frontend_present && estimated_vpp <= 0.0) {
        set_reason(g, "no front end and unknown input voltage");
        if (reason && reason_len) snprintf(reason, reason_len, "%s", g->reason);
        return AS_ERR_RANGE;
    }

    g->input_mode = input_mode;
    g->estimated_vpp = estimated_vpp;
    g->frontend_present = frontend_present;
    g->clip_channel = -1;
    g->clip_peak = 0.0;
    g->state = AS_SAFETY_PRECHECK;
    if (estimated_vpp <= 0.0)
        set_reason(g, "precheck passed: voltage unknown, use caution");
    else if (!frontend_present)
        set_reason(g, "precheck passed: no front end, low-voltage input only");
    else
        set_reason(g, "precheck passed");
    if (reason && reason_len) snprintf(reason, reason_len, "%s", g->reason);
    return AS_OK;
}

AsStatus as_safety_arm(AsSafetyGuard *g) {
    if (!g) return AS_ERR_INVALID;
    if (g->state != AS_SAFETY_PRECHECK) {
        set_reason(g, "arm requires PRECHECK");
        return AS_ERR_STATE;
    }
    g->state = AS_SAFETY_ARMED;
    set_reason(g, "armed: output muted, input at minimum range");
    return AS_OK;
}

AsStatus as_safety_start(AsSafetyGuard *g) {
    if (!g) return AS_ERR_INVALID;
    if (g->state != AS_SAFETY_ARMED) {
        set_reason(g, "start requires ARMED");
        return AS_ERR_STATE;
    }
    g->state = AS_SAFETY_RUNNING;
    set_reason(g, "running");
    return AS_OK;
}

AsStatus as_safety_clip(AsSafetyGuard *g, int channel, double peak) {
    if (!g) return AS_ERR_INVALID;
    if (g->state != AS_SAFETY_RUNNING) {
        set_reason(g, "clip latch only while RUNNING");
        return AS_ERR_STATE;
    }
    g->state = AS_SAFETY_FAULT;
    g->clip_channel = channel;
    g->clip_peak = peak;
    snprintf(g->reason, sizeof(g->reason),
             "clip latched: ch%d peak %.4f; output muted", channel, peak);
    return AS_OK;
}

AsStatus as_safety_reset(AsSafetyGuard *g) {
    if (!g) return AS_ERR_INVALID;
    if (g->state != AS_SAFETY_FAULT) {
        set_reason(g, "reset requires FAULT");
        return AS_ERR_STATE;
    }
    g->state = AS_SAFETY_IDLE;
    g->clip_channel = -1;
    g->clip_peak = 0.0;
    set_reason(g, "reset; ready");
    return AS_OK;
}

AsSafetyState as_safety_state(const AsSafetyGuard *g) {
    return g ? g->state : AS_SAFETY_IDLE;
}

const char *as_safety_state_name(AsSafetyState state) {
    switch (state) {
    case AS_SAFETY_IDLE: return "IDLE";
    case AS_SAFETY_PRECHECK: return "PRECHECK";
    case AS_SAFETY_ARMED: return "ARMED";
    case AS_SAFETY_RUNNING: return "RUNNING";
    case AS_SAFETY_FAULT: return "FAULT";
    default: return "?";
    }
}

const char *as_safety_reason(const AsSafetyGuard *g) {
    return g ? g->reason : "";
}
