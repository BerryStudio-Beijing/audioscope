/* AudioScope by BerryStudio 0.9 — native app model and engine. */
#include "app/native_model.hpp"

#include "core/api.h"
#include "core/pdf.h"
#include "core/phase.h"
#include "core/repair.h"
#include "core/report.h"
#include "core/window.h"
#include "platform/audio_backend.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <system_error>
#include <chrono>
#include <thread>

namespace audioscope {
namespace {

constexpr int kHistSize = 16384;   /* 最长 FFT 窗也是 16384 */
constexpr int kScopeSamples = 720;
constexpr int kSpecBins = 220;
constexpr double kPi = 3.14159265358979323846;

std::string home_dir() {
    const char *home = std::getenv("HOME");
    if (!home || !*home) home = std::getenv("USERPROFILE");
    return home ? std::string(home) : std::string(".");
}

std::string device_label(const std::vector<NativeDevice> &devs, int sel,
                         bool input) {
    if (sel < 0 || sel >= (int)devs.size()) return "system default";
    const NativeDevice &d = devs[(size_t)sel];
    if (input ? !d.is_input : !d.is_output) return "system default";
    return d.name;
}

} // namespace

NativeEngine::NativeEngine() {
    hist_.assign((size_t)kHistSize, 0.0f);
    xy1_.assign((size_t)kHistSize, 0.0f);
    xy2_.assign((size_t)kHistSize, 0.0f);
    /* 采集/合成缓冲只分配一次：0.9 的 tick() 每帧都构造 3 个 vector，
     * 60 fps 下就是每秒 180 次堆分配。 */
    a_.assign((size_t)kScopeSamples, 0.0f);
    b_.assign((size_t)kScopeSamples, 0.0f);
    inter_.assign((size_t)kScopeSamples * 8u, 0.0f);
    fft_alloc(4096);   /* 默认 4096；面板可改成 1024–16384 */
    slow_count_ = slow_div_;   /* 第一帧就给出频谱，不留空图 */
}

bool NativeEngine::fft_alloc(int n) {
    if (n < 1024 || n > kHistSize || (n & (n - 1)) != 0) return false;
    if (fft_ok_) {
        as_fft_free(&fft_);
        fft_ok_ = false;
    }
    fft_size_ = n;
    win_.assign((size_t)n, 0.0);
    fft_re_.assign((size_t)n, 0.0);
    mag_.assign((size_t)n / 2 + 1, 0.0);
    build_bins(48000.0);
    fft_ok_ = as_fft_init(&fft_, n) == 0;
    if (fft_ok_) as_window_hann(win_.data(), n);
    return fft_ok_;
}

void NativeEngine::build_bins(double fs) {
    const int n = fft_size_ > 0 ? fft_size_ : 4096;
    const double rate = fs >= 8000.0 ? fs : 48000.0;
    bin_idx_.assign((size_t)kSpecBins, 1);
    bin_freq_.assign((size_t)kSpecBins, 20.0);
    for (int i = 0; i < kSpecBins; ++i) {
        const double f =
            20.0 * std::pow(1000.0, (double)i / (double)(kSpecBins - 1));
        int bin = (int)std::lround(f * (double)n / rate);
        bin_idx_[(size_t)i] = std::clamp(bin, 1, n / 2 - 1);
        bin_freq_[(size_t)i] = f;
    }
    fft_rate_ = rate;
}

NativeEngine::~NativeEngine() {
    if (audio_) as_audio_close((AsAudio *)audio_);
    if (fft_ok_) as_fft_free(&fft_);
}

void NativeEngine::synth_frames(float *ch1, float *ch2, int n) {
    double freq = 1000.0 + 25.0 * std::sin(time_ * 0.7);
    double amp = 0.5 * (0.985 + 0.015 * std::sin(time_ * 1.3));
    for (int i = 0; i < n; ++i) {
        double ph = phase_ + 2.0 * kPi * freq * (double)i / 48000.0;
        ch1[i] = (float)(amp * std::sin(ph) + 0.00015 * std::sin(3.0 * ph));
        ch2[i] = (float)(0.9 * amp * std::sin(ph + kPi / 4.0) +
                         0.00012 * std::sin(3.0 * ph + kPi / 4.0));
    }
    phase_ += 2.0 * kPi * freq * (double)n / 48000.0;
    if (phase_ > 2.0 * kPi) phase_ = std::fmod(phase_, 2.0 * kPi);
    time_ += (double)n / 48000.0;
}

void NativeEngine::push_audio(const float *ch1, const float *ch2, int n) {
    if (n <= 0) return;
    if (n > (int)hist_.size()) n = (int)hist_.size();
    std::memmove(hist_.data(), hist_.data() + n,
                 (hist_.size() - (size_t)n) * sizeof(float));
    std::memcpy(hist_.data() + (hist_.size() - (size_t)n), ch1,
                (size_t)n * sizeof(float));
    std::memmove(xy1_.data(), xy1_.data() + n,
                 (xy1_.size() - (size_t)n) * sizeof(float));
    std::memmove(xy2_.data(), xy2_.data() + n,
                 (xy2_.size() - (size_t)n) * sizeof(float));
    std::memcpy(xy1_.data() + (xy1_.size() - (size_t)n), ch1,
                (size_t)n * sizeof(float));
    std::memcpy(xy2_.data() + (xy2_.size() - (size_t)n), ch2,
                (size_t)n * sizeof(float));
}

void NativeEngine::tick(NativeModel *model, double dt) {
    if (!model || model->hold) return;
    if (dt < 0.0) dt = 0.0;

    if (fft_size_ > 0 && model->fft_size != fft_size_)
        set_fft_size(model, model->fft_size);

    /* 面板改动在 UI 线程置位，这里统一消费，避免从两个线程碰音频句柄。 */
    if (model->gen_dirty) {
        model->gen_dirty = false;
        apply_generator(model, nullptr);
    }
    if (model->io_dirty) {
        model->io_dirty = false;
        apply_io(model, nullptr);
    }
    /* 设备热插拔：最多每 2 s 重新枚举一次（CoreAudio 枚举很便宜，但没必要每帧）。 */
    device_check_s_ += dt;
    if (device_check_s_ >= 2.0) {
        device_check_s_ = 0.0;
        refresh_devices(model);
    }

    model->frame++;
    const double tick_fs = model->fs > 8000.0 ? model->fs : 48000.0;
    model->seconds += (double)kScopeSamples / tick_fs;

    if (capture_ && audio_) {
        const int want = kScopeSamples * cap_channels_;
        if ((int)inter_.size() < want) inter_.assign((size_t)want, 0.0f);
        int got = as_audio_read((AsAudio *)audio_, inter_.data(), kScopeSamples);
        if (got > 0 && got <= kScopeSamples) {
            for (int i = 0; i < got; ++i) {
                a_[(size_t)i] = inter_[(size_t)i * (size_t)cap_channels_];
                b_[(size_t)i] = cap_channels_ > 1
                                    ? inter_[(size_t)i * (size_t)cap_channels_ + 1]
                                    : a_[(size_t)i];
            }
            push_audio(a_.data(), b_.data(), got);
        }
    } else {
        synth_frames(a_.data(), b_.data(), kScopeSamples);
        push_audio(a_.data(), b_.data(), kScopeSamples);
    }

    compute(model);
    /* 波形/读数每帧更新；FFT、THD、相位按 1/3 降频（60 fps → 20 Hz）。
     * 这三项各含一次 4096 点 FFT，是原来的主要开销。 */
    if (++slow_count_ >= slow_div_) {
        slow_count_ = 0;
        compute_spectrum(model);
    }
}

void NativeEngine::compute(NativeModel *model) {
    const double fs = model->fs > 8000.0 ? model->fs : 48000.0;
    model->wave.assign(hist_.end() - kScopeSamples, hist_.end());
    as_stats_compute(model->wave.data(), kScopeSamples, fs, &model->stats);

    int clip1 = 0;
    for (float v : model->wave)
        if (std::fabs(v) >= 0.999f) { clip1 = 1; break; }

    /* CH2 独立统计：不再由 CH1 减 0.9 dB 推算。 */
    const int channels = std::clamp(model->channels, 1, 8);
    AsStats st2{};
    int clip2 = 0;
    if (channels > 1) {
        const float *tail2 = xy2_.data() + (kHistSize - kScopeSamples);
        as_stats_compute(tail2, kScopeSamples, fs, &st2);
        for (int i = 0; i < kScopeSamples; ++i)
            if (std::fabs(tail2[i]) >= 0.999f) { clip2 = 1; break; }
    }
    model->clip = clip1 || clip2;

    model->xy_x.resize(512);
    model->xy_y.resize(512);
    for (int i = 0; i < 512; ++i) {
        model->xy_x[(size_t)i] = xy1_[(size_t)(kHistSize - 512 + i)];
        model->xy_y[(size_t)i] = xy2_[(size_t)(kHistSize - 512 + i)];
    }

    for (int c = 0; c < 8; ++c) model->ch[c] = NativeChannel{};
    model->ch[0].active = true;
    model->ch[0].peak_db = as_linear_to_db(model->stats.peak);
    model->ch[0].rms_db = as_linear_to_db(model->stats.rms);
    model->ch[0].freq = model->stats.freq;
    model->ch[0].vpp = model->stats.vpp;
    model->ch[0].clip = clip1;
    model->ch[0].stats = model->stats;
    if (channels > 1) {
        model->ch[1].active = true;
        model->ch[1].peak_db = as_linear_to_db(st2.peak);
        model->ch[1].rms_db = as_linear_to_db(st2.rms);
        model->ch[1].freq = st2.freq;
        model->ch[1].vpp = st2.vpp;
        model->ch[1].clip = clip2;
        model->ch[1].stats = st2;
    }
}

void NativeEngine::compute_spectrum(NativeModel *model) {
    if (!fft_ok_) return;
    const double fs = model->fs > 8000.0 ? model->fs : 48000.0;
    if (std::fabs(fs - fft_rate_) > 0.5) build_bins(fs);
    const int n = fft_size_ > 0 ? fft_size_ : 4096;
    const float *tail = hist_.data() + (kHistSize - n);
    model->spec_freq.resize((size_t)kSpecBins);
    model->spec_db.resize((size_t)kSpecBins);
    for (int i = 0; i < n; ++i)
        fft_re_[(size_t)i] = (double)tail[i] * win_[(size_t)i];
    as_fft_forward(&fft_, fft_re_.data(), mag_.data(), n);
    double maxmag = 1e-12;
    for (int i = 1; i < n / 2; ++i)
        maxmag = std::max(maxmag, mag_[(size_t)i]);
    for (int i = 0; i < kSpecBins; ++i) {
        double db = 20.0 *
                    std::log10(std::max(mag_[(size_t)bin_idx_[(size_t)i]] / maxmag,
                                        1e-6));
        model->spec_freq[(size_t)i] = bin_freq_[(size_t)i];
        model->spec_db[(size_t)i] = std::max(db, -100.0);
    }
    as_tone_analyze(tail, n, fs, &model->tone);

    AsPhaseResult pr{};
    if (as_phase_measure(tail, xy2_.data() + (kHistSize - n), n, fs, &pr) == 0) {
        model->phase_deg = pr.phase_deg;
        model->gain_db = pr.gain_db;
    }
}

void NativeEngine::update_xy(NativeModel *model) { (void)model; }

/* --------------------------------------------------------- 0.9.3 audio I/O */

void NativeEngine::refresh_devices(NativeModel *model) {
    if (!model) return;
    AsDeviceInfo raw[64];
    const int n = as_platform_list_devices(raw, 64);
    model->devices.clear();
    if (n > 0) model->devices.reserve((size_t)n);
    for (int i = 0; i < n; ++i) {
        NativeDevice d;
        d.id = raw[i].id;
        d.name = raw[i].name;
        d.is_input = raw[i].is_input != 0;
        d.is_output = raw[i].is_output != 0;
        d.in_channels = raw[i].in_channels;
        d.out_channels = raw[i].out_channels;
        d.rate = raw[i].sample_rate;
        model->devices.push_back(std::move(d));
    }

    /* 稳定 id 优先：设备换了位置也能找回；找不回就退回系统默认，不猜。 */
    const std::string want_in = model->in_id;
    auto locate = [&](int &sel, std::string &id, bool input) {
        if (sel < 0 || sel >= (int)model->devices.size()) {
            sel = -1;
            id.clear();
            return false;
        }
        const NativeDevice &cur = model->devices[(size_t)sel];
        if (input ? !cur.is_input : !cur.is_output) {
            sel = -1;
            id.clear();
            return false;
        }
        if (id.empty() || cur.id == id) {
            id = cur.id;
            return true;
        }
        for (size_t k = 0; k < model->devices.size(); ++k) {
            const NativeDevice &d = model->devices[k];
            if (d.id != id) continue;
            if (input ? !d.is_input : !d.is_output) continue;
            sel = (int)k;
            return true;
        }
        sel = -1;
        id.clear();
        return false;
    };
    locate(model->sel_in, model->in_id, true);
    locate(model->sel_out, model->out_id, false);

    const bool in_lost = !want_in.empty() && model->in_id.empty();
    if (in_lost && capture_) {
        /* 硬约束：输入模式不得静默切换到别的设备。停采，等用户重新选择。 */
        capture_ = false;
        model->capture = false;
        model->source = "synthetic";
        model->device = "synthetic";
        model->device_lost = true;
        model->message = "input device lost — capture stopped";
        if (audio_) {
            as_audio_close((AsAudio *)audio_);
            audio_ = nullptr;
        }
        if (model->gen_on) reopen_stream(model, nullptr);
    } else if (model->capture) {
        model->device = device_label(model->devices, model->sel_in, true);
    }

    model->io_status = "in: " + device_label(model->devices, model->sel_in, true) +
                       " · out: " +
                       device_label(model->devices, model->sel_out, false);
    if (model->devices.empty())
        model->io_status = "no devices — system default";
}

AsGenConfig NativeEngine::gen_config(const NativeModel *model) const {
    AsGenConfig cfg{};
    if (!model) return cfg;
    cfg.wave = model->gen_wave;
    cfg.freq_hz = model->gen_hz;
    cfg.freq_end_hz = model->gen_hz_end;
    cfg.period_s = model->gen_period;
    /* 安全不变量：默认输出 −20 dBFS；只有显式解锁才允许更高。 */
    double db = model->gen_db;
    if (!model->gen_hot_allowed && db > -20.0) db = -20.0;
    if (db > 0.0) db = 0.0;
    if (db < -80.0) db = -80.0;
    cfg.amp = std::pow(10.0, db / 20.0);
    return cfg;
}

bool NativeEngine::reopen_stream(NativeModel *model, std::string *error) {
    if (audio_) {
        as_audio_close((AsAudio *)audio_);
        audio_ = nullptr;
    }
    if (!model) return false;

    const bool want_in = capture_;
    const bool want_out = model->gen_on;
    if (!want_in && !want_out) return true;

    /* -1 = 系统默认；-2 = 不打开该方向。 */
    const int in_index = want_in ? model->sel_in : -2;
    const int out_index = want_out ? model->sel_out : -2;
    const double fs = model->rate_hz >= 8000.0 ? model->rate_hz : 48000.0;
    const int block = model->block > 0 ? model->block : 256;

    AsAudio *a = as_audio_open(in_index, out_index, fs, block);
    if (!a) {
        if (error) *error = as_audio_last_error();
        capture_ = false;
        model->capture = false;
        if (model->source == "capture") model->source = "synthetic";
        if (want_out) model->gen_on = false;
        return false;
    }
    audio_ = a;

    if (want_in) {
        cap_channels_ = std::clamp(as_audio_channels(a), 1, 8);
        const double rate = as_audio_rate(a);
        cap_fs_ = rate > 0.0 ? rate : fs;
        model->channels = cap_channels_;
        model->fs = cap_fs_;
        model->capture = true;
        model->source = "capture";
        model->device = device_label(model->devices, model->sel_in, true);
    }
    if (want_out) {
        const AsGenConfig cfg = gen_config(model);
        if (as_audio_set_generator(a, &cfg, 1) != 0) {
            if (error) *error = as_audio_last_error();
            model->gen_on = false;
        }
    }
    return true;
}

bool NativeEngine::apply_generator(NativeModel *model, std::string *error) {
    if (!model) return false;
    if (!audio_ && model->gen_on && !reopen_stream(model, error)) return false;

    const AsGenConfig cfg = gen_config(model);
    if (audio_ && as_audio_set_generator((AsAudio *)audio_, &cfg,
                                        model->gen_on ? 1 : 0) != 0) {
        if (error) *error = as_audio_last_error();
        model->gen_on = false;
        return false;
    }
    /* 发生器关掉且不在采集时释放输出设备，别一直占着声卡。 */
    if (!model->gen_on && !capture_ && audio_) {
        as_audio_close((AsAudio *)audio_);
        audio_ = nullptr;
    }

    if (model->gen_on) {
        char msg[128];
        const double db = 20.0 * std::log10(std::max(cfg.amp, 1e-9));
        std::snprintf(msg, sizeof(msg), "generator %s · %.0f Hz · %.1f dBFS",
                      as_wave_id(cfg.wave), cfg.freq_hz, db);
        model->message = msg;
    } else {
        model->message = "generator off";
    }
    return true;
}

bool NativeEngine::apply_io(NativeModel *model, std::string *error) {
    if (!model) return false;

    if (model->sel_in >= 0 && model->sel_in < (int)model->devices.size()) {
        const NativeDevice &d = model->devices[(size_t)model->sel_in];
        if (d.is_input) model->in_id = d.id;
        else model->sel_in = -1;
    }
    if (model->sel_in < 0) model->in_id.clear();

    if (model->sel_out >= 0 && model->sel_out < (int)model->devices.size()) {
        const NativeDevice &d = model->devices[(size_t)model->sel_out];
        if (d.is_output) model->out_id = d.id;
        else model->sel_out = -1;
    }
    if (model->sel_out < 0) model->out_id.clear();

    model->device_lost = false;
    if ((capture_ || model->gen_on) && !reopen_stream(model, error)) {
        model->io_status = "audio I/O failed";
        return false;
    }

    model->io_status = "in: " + device_label(model->devices, model->sel_in, true) +
                       " · out: " +
                       device_label(model->devices, model->sel_out, false);
    model->message = "audio I/O applied";
    return true;
}

bool NativeEngine::set_fft_size(NativeModel *model, int n) {
    if (!fft_alloc(n)) {
        if (model) model->fft_size = fft_size_ > 0 ? fft_size_ : 4096;
        return false;
    }
    if (model) model->fft_size = n;
    slow_count_ = slow_div_; /* 下一帧立刻用新窗刷新频谱 */
    return true;
}


bool NativeEngine::set_capture(NativeModel *model, bool on, std::string *error) {
    if (on == capture_) return true;
    if (!on) {
        capture_ = false;
        if (!(model && model->gen_on)) {
            if (audio_) {
                as_audio_close((AsAudio *)audio_);
                audio_ = nullptr;
            }
        } else {
            reopen_stream(model, nullptr); /* 保留发生器输出 */
        }
        if (model) {
            model->capture = false;
            model->source = "synthetic";
            model->device = "synthetic";
            model->message = "capture stopped";
        }
        return true;
    }
    capture_ = true;
    if (!reopen_stream(model, error)) {
        capture_ = false;
        return false;
    }
    if (model) {
        model->device_lost = false;
        model->message = "capturing input";
    }
    return true;
}

void NativeEngine::run_repair(NativeModel *model) {
    if (!model) return;
    model->plan.clear();
    /* 3 kHz tape fixture with 0.3 % wow and 0.1 % flutter. */
    const int n = 10 * 48000;
    std::vector<float> tone((size_t)n);
    double ph = 0.0;
    for (int i = 0; i < n; ++i) {
        double t = (double)i / 48000.0;
        double mod = 1.0 + 0.003 * std::sin(2.0 * kPi * 0.55 * t) +
                     0.001 * std::sin(2.0 * kPi * 6.0 * t);
        ph += 2.0 * kPi * 3000.0 * mod / 48000.0;
        tone[(size_t)i] = (float)(0.5 * std::sin(ph));
    }
    AsWowResult wow{};
    as_wow_measure(tone.data(), n, 48000.0, &wow);

    /* 10 kHz stereo fixture, 0.4 deg azimuth error. */
    std::vector<float> la(48000), lb(48000);
    for (int i = 0; i < 48000; ++i) {
        double p = 2.0 * kPi * 10000.0 * (double)i / 48000.0;
        la[(size_t)i] = (float)(0.4 * std::sin(p));
        lb[(size_t)i] = (float)(0.4 * std::sin(p + 0.4 * kPi / 180.0));
    }
    AsPhaseResult az{};
    as_phase_measure(la.data(), lb.data(), 48000, 48000.0, &az);

    double thd = model->tone.thd_pct;
    auto add = [&](const char *label, double value, const char *unit, bool pass) {
        NativePlanStep s;
        s.label = label;
        s.value = value;
        s.unit = unit;
        s.pass = pass;
        model->plan.push_back(s);
        if (!pass) model->plan_pass = false;
    };
    model->plan_pass = true;
    add("Wow", wow.wow_pct, "% RMS", wow.wow_pct < 0.5);
    add("Flutter", wow.flutter_pct, "% RMS", wow.flutter_pct < 0.2);
    add("Azimuth", std::fabs(az.phase_deg), "deg", std::fabs(az.phase_deg) < 5.0);
    add("THD", thd, "%", thd < 0.1);
    model->plan_ran = true;
    model->plan_simulated = true;  /* 固定夹具演示，不是被测设备结果 */
    model->message = model->plan_pass ? "simulated plan PASS (fixtures)"
                                      : "simulated plan FAIL (fixtures)";
}

bool NativeEngine::run_loopback(NativeModel *model, std::string *error) {
    if (!model) return false;
    const double fs = model->rate_hz >= 8000.0 ? model->rate_hz : 48000.0;
    AsAudio *a = as_audio_open(model->sel_in, model->sel_out, fs,
                               model->block > 0 ? model->block : 256);
    if (!a) {
        if (error) *error = as_audio_last_error();
        return false;
    }
    AsGenConfig cfg{};
    cfg.wave = AS_WAVE_SINE;
    cfg.freq_hz = 1000.0;
    cfg.freq_end_hz = 1000.0;
    cfg.amp = 0.1; /* −20 dBFS：与安全默认一致 */
    cfg.period_s = 1.0;
    if (as_audio_set_generator(a, &cfg, 1) != 0) {
        as_audio_close(a);
        if (error) *error = "cannot start loopback tone";
        return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    const double actual = as_audio_rate(a) > 8000.0 ? as_audio_rate(a) : fs;
    int channels = std::clamp(as_audio_channels(a), 1, 8);
    std::vector<float> buf((size_t)480 * (size_t)channels, 0.0f);
    std::vector<float> mono;
    const size_t target = (size_t)(actual * 2.0);
    mono.reserve(target);
    auto start = std::chrono::steady_clock::now();
    while (mono.size() < target) {
        int got = as_audio_read(a, buf.data(), 480);
        for (int i = 0; i < got; ++i) mono.push_back(buf[(size_t)i * (size_t)channels]);
        auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        if (elapsed > 3.5) break;
        if (got <= 0) std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    as_audio_set_generator(a, &cfg, 0);
    as_audio_close(a);

    if (mono.size() < (size_t)(actual * 0.1)) {
        if (error) *error = "loopback captured too few frames";
        return false;
    }
    AsStats st{};
    AsToneResult tone{};
    as_stats_compute(mono.data(), (int)mono.size(), actual, &st);
    as_tone_analyze(mono.data(), (int)mono.size(), actual, &tone);
    model->loopback_ran = true;
    model->loopback_thd = tone.thd_pct;
    model->loopback_level = as_linear_to_db(st.rms);
    model->loopback_pass = model->loopback_level > -60.0 && tone.thd_pct < 1.0;
    model->message = model->loopback_pass ? "loopback connectivity OK"
                                          : "loopback connectivity FAIL";
    return true;
}

std::string NativeEngine::report_dir() const {
    std::filesystem::path p;
#ifdef _WIN32
    const char *local = std::getenv("LOCALAPPDATA");
    p = local && *local ? std::filesystem::path(local) : std::filesystem::path(home_dir());
#else
    p = std::filesystem::path(home_dir()) / "Library" / "Application Support";
#endif
    p /= "AudioScope";
    p /= "reports";
    std::error_code ec;
    std::filesystem::create_directories(p, ec);
    return p.string();
}

bool NativeEngine::export_report(NativeModel *model, std::string *pdf_path,
                                 std::string *error) {
    if (!model) return false;
    if (!model->plan_ran) run_repair(model);

    AsReport r;
    as_report_init(&r, model->device.c_str(), "Inst", model->fs, kScopeSamples,
                   (double)kScopeSamples / model->fs);
    r.confidence = AS_CONF_ASSUMED;
    std::snprintf(r.calibration_id, sizeof(r.calibration_id), "%s",
                  model->cal_id.c_str());
    r.fs_volts_rms = model->cal_fs_vrms;
    r.channels = std::clamp(model->channels, 1, 8);
    for (int c = 0; c < 2; ++c) {
        AsChannelMeasurement *m = &r.ch[c];
        std::snprintf(m->label, sizeof(m->label), "CH%d", c + 1);
        m->channel = c;
        m->stats = model->ch[c].active ? model->ch[c].stats : AsStats{};
        m->peak_dbfs = model->ch[c].peak_db;
        m->rms_dbfs = model->ch[c].rms_db;
        m->vpp = model->ch[c].vpp;
        m->clip_count = model->ch[c].clip;
    }
    as_report_evaluate(&r);
    if (model->plan_simulated) {
        /* 夹具结果不能以“实测 PASS”发布：降为 WARN 并在文本/备注里写明。 */
        std::snprintf(r.notes, sizeof(r.notes),
                      "SIMULATED FIXTURES - not a measurement of the device under test");
        char verdict[sizeof(r.verdict_text)];
        std::snprintf(verdict, sizeof(verdict), "SIMULATED: %s", r.verdict_text);
        std::snprintf(r.verdict_text, sizeof(r.verdict_text), "%s", verdict);
        if (r.verdict == 0) r.verdict = 1;
    }

    std::string dir = report_dir();
    std::string base = std::string(r.run_id);
    std::string json = dir + "/" + base + ".json";
    std::string html = dir + "/" + base + ".html";
    std::string pdf = dir + "/" + base + ".pdf";
    int rc = as_report_write_json(&r, json.c_str());
    if (rc == 0) rc = as_report_write_html(&r, html.c_str());
    if (rc == 0)
        rc = as_report_write_pdf_ex(&r, model->wave.data(), (int)model->wave.size(),
                                    model->spec_db.data(), (int)model->spec_db.size(),
                                    pdf.c_str());
    if (rc != 0) {
        if (error) *error = "cannot write report files";
        return false;
    }

    NativeReport rep;
    rep.name = base + ".pdf";
    rep.kind = "measurement";
    rep.time = r.timestamp;
    rep.pass = r.verdict == 0;
    model->reports.insert(model->reports.begin(), rep);
    if (model->reports.size() > 8) model->reports.resize(8);
    model->last_report = pdf;
    if (pdf_path) *pdf_path = pdf;
    model->message = "report written";
    return true;
}

} // namespace audioscope
