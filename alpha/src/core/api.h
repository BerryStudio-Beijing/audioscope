/* AudioScope by BerryStudio 0.7-alpha — stable C ABI for the portable measurement core.
 *
 * This header is the boundary between the C11 realtime core and the C++17
 * application/platform layers. It must stay includable from both C and C++,
 * and it must never expose STL types or C++ exceptions.
 */
#ifndef AS_CORE_API_H
#define AS_CORE_API_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AS_BRAND "AudioScope by BerryStudio"
#define AS_VERSION "0.8.0-alpha.1"
#define AS_VERSION_MAJOR 0
#define AS_VERSION_MINOR 8
#define AS_VERSION_PATCH 0
#define AS_ALPHA_VERSION AS_VERSION

typedef enum {
    AS_OK = 0,
    AS_ERR_INVALID = -1,
    AS_ERR_STATE = -2,
    AS_ERR_IO = -3,
    AS_ERR_NOT_IMPLEMENTED = -4,
    AS_ERR_RANGE = -5
} AsStatus;

const char *as_brand_string(void);
const char *as_version_string(void);
const char *as_status_string(AsStatus status);

/* ------------------------------------------------------------------ */
/* Units                                                               */
/* ------------------------------------------------------------------ */

typedef enum {
    AS_UNIT_DBFS = 0,
    AS_UNIT_DBU,
    AS_UNIT_DBV,
    AS_UNIT_VRMS,
    AS_UNIT_VPP
} AsUnit;

const char *as_unit_name(AsUnit unit);

/* Convert between units. `fs_volts_rms` is the input full-scale voltage;
 * `crest` is peak/RMS (use 1.41421356 for a sine, 1.0 for DC). Returns
 * AS_ERR_INVALID for unknown units or non-positive full scale. */
AsStatus as_units_convert(double value, AsUnit from, AsUnit to,
                          double fs_volts_rms, double crest, double *out);

/* ------------------------------------------------------------------ */
/* Input modes                                                         */
/* ------------------------------------------------------------------ */

typedef enum {
    AS_MODE_LINE_PRO = 0,
    AS_MODE_LINE_CONSUMER,
    AS_MODE_DI,
    AS_MODE_INST,
    AS_MODE_MIC,
    AS_MODE_PROBE,
    AS_MODE_COUNT
} AsInputMode;

typedef struct {
    const char *id;
    const char *label;
    double nominal_dbu;
    double max_vpp;
    double input_impedance_ohm;
    int phantom_off_required;
    int frontend_required;
} AsInputModeSpec;

const AsInputModeSpec *as_input_mode_spec(AsInputMode mode);
int as_input_mode_parse(const char *id);
const char *as_input_mode_id(AsInputMode mode);

/* ------------------------------------------------------------------ */
/* Safety guard                                                        */
/* ------------------------------------------------------------------ */

typedef enum {
    AS_SAFETY_IDLE = 0,
    AS_SAFETY_PRECHECK,
    AS_SAFETY_ARMED,
    AS_SAFETY_RUNNING,
    AS_SAFETY_FAULT
} AsSafetyState;

typedef struct AsSafetyGuard AsSafetyGuard;

AsSafetyGuard *as_safety_create(void);
void as_safety_destroy(AsSafetyGuard *guard);

/* `input_mode`: 0=line, 1=consumer, 2=di, 3=inst, 4=mic, 5=probe.
 * `estimated_vpp`: user estimate; <= 0 means unknown.
 * `frontend_present`: whether the protected front end is connected. */
AsStatus as_safety_precheck(AsSafetyGuard *guard, int input_mode,
                            double estimated_vpp, int frontend_present,
                            char *reason, size_t reason_len);
AsStatus as_safety_arm(AsSafetyGuard *guard);
AsStatus as_safety_start(AsSafetyGuard *guard);
AsStatus as_safety_clip(AsSafetyGuard *guard, int channel, double peak);
AsStatus as_safety_reset(AsSafetyGuard *guard);
AsStatus as_safety_stop(AsSafetyGuard *guard);

/* Scan interleaved stereo frames for clipping. Latches FAULT on the first
 * overloaded frame and returns the offending channel/peak. */
AsStatus as_safety_feed(AsSafetyGuard *guard, const float *interleaved,
                        int frames, int channels, int *clip_channel,
                        double *clip_peak);

/* Recommend a front-end range for an estimated Vpp. */
AsStatus as_safety_recommend_range(double vpp, int frontend_present,
                                   char *out, size_t out_len);

AsSafetyState as_safety_state(const AsSafetyGuard *guard);
const char *as_safety_state_name(AsSafetyState state);
const char *as_safety_reason(const AsSafetyGuard *guard);

/* ------------------------------------------------------------------ */
/* Calibration                                                         */
/* ------------------------------------------------------------------ */

typedef enum {
    AS_CONF_MEASURED = 0,
    AS_CONF_DATASHEET = 1,
    AS_CONF_ASSUMED = 2
} AsConfidence;

typedef struct {
    int schema_version;
    char device_id[96];
    int input_channel;
    double gain_db;
    double pad_db;
    int sample_rate;
    double fs_volts_rms;
    double input_impedance_ohm;
    AsConfidence confidence;
    char method[32];
    char created_at[32];
    char checksum[80];
} AsCalibration;

void as_calibration_init(AsCalibration *cal);
void as_calibration_seal(AsCalibration *cal);
int as_calibration_validate(const AsCalibration *cal, char *err, size_t err_len);
double as_calibration_volts_rms(const AsCalibration *cal, double dbfs);
int as_calibration_write_json(const AsCalibration *cal, const char *path);
int as_calibration_read_json(AsCalibration *cal, const char *path);
const char *as_confidence_name(AsConfidence confidence);

/* ------------------------------------------------------------------ */
/* Session log                                                         */
/* ------------------------------------------------------------------ */

typedef struct AsSession AsSession;

/* Creates `dir` if needed and appends to `dir/session.jsonl`. */
AsSession *as_session_create(const char *dir, const char *session_id);
void as_session_destroy(AsSession *session);
/* `data_json` must be a valid JSON value, e.g. "{}" or "{\"key\":1}". */
int as_session_event(AsSession *session, const char *type,
                     const char *data_json);
const char *as_session_path(const AsSession *session);
int as_session_count(const AsSession *session);

/* ------------------------------------------------------------------ */
/* Platform device enumeration                                         */
/* ------------------------------------------------------------------ */

struct AsAudio;

typedef struct {
    char id[96];
    char name[160];
    int is_input;
    int is_output;
    int channels;       /* max(in, out), kept for compatibility */
    int in_channels;
    int out_channels;
    double sample_rate;
} AsDeviceInfo;

int as_platform_list_devices(AsDeviceInfo *out, int max);
const char *as_platform_name(void);

/* Number of interleaved input channels returned by as_audio_read(). */
int as_audio_channels(const struct AsAudio *audio);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* AS_CORE_API_H */
