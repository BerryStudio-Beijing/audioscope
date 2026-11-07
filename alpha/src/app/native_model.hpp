/* AudioScope by BerryStudio 0.9 — native app model and engine.
 *
 * One data model drives both the macOS (AppKit/CoreGraphics) and Windows
 * (Win32/GDI+) front ends. It owns no platform UI code; the engine only uses
 * the C core and the platform audio backend.
 */
#ifndef AS_APP_NATIVE_MODEL_HPP
#define AS_APP_NATIVE_MODEL_HPP

#include "core/fft.h"
#include "core/stats.h"
#include "core/tone.h"

#include <cstdint>
#include <string>
#include <vector>

namespace audioscope {

enum NativePage {
    PAGE_SCOPE = 0,
    PAGE_SPECTRUM,
    PAGE_XY,
    PAGE_MEASURE,
    PAGE_REPAIR,
    PAGE_REPORTS,
    PAGE_CALIBRATION,
    PAGE_COUNT
};

struct NativeChannel {
    bool active = false;
    double peak_db = -120.0;
    double rms_db = -120.0;
    double freq = 0.0;
    double vpp = 0.0;
    int clip = 0;
};

struct NativePlanStep {
    std::string label;
    double value = 0.0;
    std::string unit;
    bool pass = false;
};

struct NativeReport {
    std::string name;
    std::string kind;
    std::string time;
    bool pass = true;
};

struct NativeModel {
    int page = PAGE_SCOPE;
    bool light = false;
    bool hold = false;
    bool running = true;
    bool capture = false;
    bool capture_available = false;
    std::string source = "synthetic";
    std::string device = "synthetic";
    std::string platform = "macos-coreaudio";
    double fs = 48000.0;
    int channels = 2;
    uint64_t frame = 0;
    double seconds = 0.0;

    std::vector<float> wave;       /* 720 CH1 samples */
    AsStats stats{};
    int clip = 0;

    std::vector<double> spec_db;   /* 220 log bins, -100..0 dB */
    std::vector<double> spec_freq; /* matching Hz */
    AsToneResult tone{};

    std::vector<float> xy_x;       /* 512 points */
    std::vector<float> xy_y;
    double phase_deg = 0.0;
    double gain_db = 0.0;

    NativeChannel ch[8];

    std::vector<NativePlanStep> plan;
    bool plan_pass = true;
    bool plan_ran = false;
    bool loopback_ran = false;
    bool loopback_pass = false;
    double loopback_thd = 0.0;
    double loopback_level = -120.0;

    std::vector<NativeReport> reports;
    std::string last_report;

    std::string cal_id = "none";
    std::string cal_confidence = "ASSUMED";
    double cal_fs_vrms = 0.0;
    std::string message = "ready";

    /* Set by the UI on click; the platform shell handles the ones that need a
     * native dialog (capture confirmation, save panel). */
    bool req_capture_toggle = false;
    bool req_export = false;
    bool req_run_repair = false;
    bool req_run_loopback = false;
};

class NativeEngine {
public:
    NativeEngine();
    ~NativeEngine();
    NativeEngine(const NativeEngine &) = delete;
    NativeEngine &operator=(const NativeEngine &) = delete;

    void tick(NativeModel *model, double dt);
    bool set_capture(NativeModel *model, bool on, std::string *error);
    void run_repair(NativeModel *model);
    bool run_loopback(NativeModel *model, std::string *error);
    bool export_report(NativeModel *model, std::string *pdf_path, std::string *error);

private:
    void synth_frames(float *ch1, float *ch2, int n);
    void push_audio(const float *ch1, const float *ch2, int n);
    void compute(NativeModel *model);
    void update_xy(NativeModel *model);
    std::string report_dir() const;

    void *audio_ = nullptr;        /* AsAudio*, kept opaque here */
    bool capture_ = false;
    int cap_channels_ = 2;
    double cap_fs_ = 48000.0;
    double phase_ = 0.0;
    double time_ = 0.0;
    std::vector<float> hist_;      /* 4096 mono history for FFT */
    std::vector<float> xy1_, xy2_; /* 4096 stereo history for XY */
    std::vector<double> win_;
    std::vector<double> fft_re_;
    std::vector<double> mag_;
    AsFft fft_{};
    bool fft_ok_ = false;
};

} // namespace audioscope

#endif
