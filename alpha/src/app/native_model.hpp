/* AudioScope by BerryStudio 0.9 — native app model and engine.
 *
 * One data model drives both the macOS (AppKit/CoreGraphics) and Windows
 * (Win32/GDI+) front ends. It owns no platform UI code; the engine only uses
 * the C core and the platform audio backend.
 */
#ifndef AS_APP_NATIVE_MODEL_HPP
#define AS_APP_NATIVE_MODEL_HPP

#include "core/api.h"
#include "core/fft.h"
#include "core/generator.h"
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

/* 设备快照：只在 UI 线程读写；音频线程不碰。 */
struct NativeDevice {
    std::string id;
    std::string name;
    bool is_input = false;
    bool is_output = false;
    int in_channels = 0;
    int out_channels = 0;
    double rate = 0.0;
};

struct NativeChannel {
    bool active = false;
    double peak_db = -120.0;
    double rms_db = -120.0;
    double freq = 0.0;
    double vpp = 0.0;
    int clip = 0;
    AsStats stats{};   /* 每通道独立统计，报告直接用它 */
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
    bool plan_simulated = true;   /* 固定夹具演示，不是 DUT 结果 */
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

    /* ---- signal generator（0.9.3） ---------------------------------- */
    int gen_wave = AS_WAVE_SINE; /* AsWaveform；与上面的波形缓冲分开命名 */
    double gen_hz = 1000.0;      /* 基频 / 扫频起点 */
    double gen_hz_end = 20000.0; /* 对数扫频终点 */
    double gen_db = -20.0;       /* 幅度 dBFS，默认 −20（安全默认） */
    double gen_period = 1.0;     /* 扫频 / 脉冲 / 阶跃周期（秒） */
    bool gen_on = false;
    bool gen_hot_allowed = false; /* 允许超过 −20 dBFS（需显式解锁） */
    bool gen_dirty = false;       /* 面板改过参数，等 engine 应用 */

    /* ---- audio I/O（0.9.3） ----------------------------------------- */
    std::vector<NativeDevice> devices;
    int sel_in = -1;             /* UI 里的下标；-1 = 系统默认 */
    int sel_out = -1;
    std::string in_id;           /* 选中设备的稳定 id（热插拔后按 id 找回） */
    std::string out_id;
    double rate_hz = 48000.0;
    int block = 512;
    int fft_size = 4096;         /* 1024 / 2048 / 4096 / 8192 / 16384 */
    bool device_lost = false;
    std::string io_status = "system default";
    bool io_dirty = false;       /* 面板点了 APPLY */

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

    /* ---- 0.9.3: generator / audio I/O ------------------------------- */
    void refresh_devices(NativeModel *model);
    bool apply_generator(NativeModel *model, std::string *error);
    bool apply_io(NativeModel *model, std::string *error);
    bool set_fft_size(NativeModel *model, int n);

private:
    void synth_frames(float *ch1, float *ch2, int n);
    void push_audio(const float *ch1, const float *ch2, int n);
    void compute(NativeModel *model);      /* 每帧：波形 / 统计 / 通道表 */
    void compute_spectrum(NativeModel *model); /* 降频：FFT / THD / 相位 */
    void update_xy(NativeModel *model);
    std::string report_dir() const;
    bool reopen_stream(NativeModel *model, std::string *error);
    AsGenConfig gen_config(const NativeModel *model) const;
    bool fft_alloc(int n);
    void build_bins(double fs);

    void *audio_ = nullptr;        /* AsAudio*, kept opaque here */
    bool capture_ = false;
    int cap_channels_ = 2;
    double cap_fs_ = 48000.0;
    double phase_ = 0.0;
    double time_ = 0.0;
    std::vector<float> hist_;      /* 16384 mono history (最长 FFT 窗) */
    std::vector<float> xy1_, xy2_; /* 16384 stereo history for XY */
    std::vector<float> a_, b_;     /* 每帧复用的采集/合成缓冲 */
    std::vector<float> inter_;     /* 交错读缓冲（最多 8 声道） */
    std::vector<double> win_;
    std::vector<double> fft_re_;
    std::vector<double> mag_;
    std::vector<int> bin_idx_;     /* 对数频率 → FFT bin，只算一次 */
    std::vector<double> bin_freq_;
    int slow_div_ = 3;             /* 60 fps 下每 3 帧做一次 FFT ≈ 20 Hz */
    int slow_count_ = 0;
    int fft_size_ = 0;             /* 当前 FFT 点数（1024–16384） */
    double fft_rate_ = 48000.0;    /* bin 映射使用的实际采样率 */
    double device_check_s_ = 2.0;  /* 设备热插拔轮询计时：首帧立即枚举 */
    AsFft fft_{};
    bool fft_ok_ = false;
};

} // namespace audioscope

#endif
