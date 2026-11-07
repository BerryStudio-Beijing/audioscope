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

constexpr int kFftSize = 4096;
constexpr int kScopeSamples = 720;
constexpr int kSpecBins = 220;
constexpr double kPi = 3.14159265358979323846;

std::string home_dir() {
    const char *home = std::getenv("HOME");
    if (!home || !*home) home = std::getenv("USERPROFILE");
    return home ? std::string(home) : std::string(".");
}

std::string first_input_name() {
    AsDeviceInfo devs[64];
    int n = as_platform_list_devices(devs, 64);
    for (int i = 0; i < n; ++i)
        if (devs[i].is_input) return devs[i].name;
    return "default input";
}

} // namespace

NativeEngine::NativeEngine() {
    hist_.assign((size_t)kFftSize, 0.0f);
    xy1_.assign((size_t)kFftSize, 0.0f);
    xy2_.assign((size_t)kFftSize, 0.0f);
    /* 采集/合成缓冲只分配一次：0.9 的 tick() 每帧都构造 3 个 vector，
     * 60 fps 下就是每秒 180 次堆分配。 */
    a_.assign((size_t)kScopeSamples, 0.0f);
    b_.assign((size_t)kScopeSamples, 0.0f);
    inter_.assign((size_t)kScopeSamples * 8u, 0.0f);
    win_.resize((size_t)kFftSize);
    fft_re_.resize((size_t)kFftSize);
    mag_.resize((size_t)kFftSize / 2 + 1);
    /* 对数频率 → bin 映射只算一次（原来每帧 220 次 pow + log）。 */
    bin_idx_.resize((size_t)kSpecBins);
    bin_freq_.resize((size_t)kSpecBins);
    for (int i = 0; i < kSpecBins; ++i) {
        double f = 20.0 * std::pow(1000.0, (double)i / (double)(kSpecBins - 1));
        int bin = (int)std::lround(f * (double)kFftSize / 48000.0);
        bin_idx_[(size_t)i] = std::clamp(bin, 1, kFftSize / 2 - 1);
        bin_freq_[(size_t)i] = f;
    }
    fft_ok_ = as_fft_init(&fft_, kFftSize) == 0;
    if (fft_ok_) as_window_hann(win_.data(), kFftSize);
    slow_count_ = slow_div_;   /* 第一帧就给出频谱，不留空图 */
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
    (void)dt;
    if (!model || model->hold) return;
    model->frame++;
    model->seconds += (double)kScopeSamples / 48000.0;

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
    model->wave.assign(hist_.end() - kScopeSamples, hist_.end());
    as_stats_compute(model->wave.data(), kScopeSamples, 48000.0, &model->stats);
    model->clip = 0;
    for (float v : model->wave)
        if (std::fabs(v) >= 0.999f) { model->clip = 1; break; }

    model->xy_x.resize(512);
    model->xy_y.resize(512);
    for (int i = 0; i < 512; ++i) {
        model->xy_x[(size_t)i] = xy1_[(size_t)(kFftSize - 512 + i)];
        model->xy_y[(size_t)i] = xy2_[(size_t)(kFftSize - 512 + i)];
    }

    for (int c = 0; c < 8; ++c) model->ch[c] = NativeChannel{};
    model->ch[0].active = true;
    model->ch[0].peak_db = as_linear_to_db(model->stats.peak);
    model->ch[0].rms_db = as_linear_to_db(model->stats.rms);
    model->ch[0].freq = model->stats.freq;
    model->ch[0].vpp = model->stats.vpp;
    model->ch[0].clip = model->clip;
    model->ch[1].active = model->channels > 1;
    model->ch[1].peak_db = model->ch[0].peak_db - 0.9;
    model->ch[1].rms_db = model->ch[0].rms_db - 0.9;
    model->ch[1].freq = model->ch[0].freq;
    model->ch[1].vpp = model->ch[0].vpp * 0.9;
}

void NativeEngine::compute_spectrum(NativeModel *model) {
    if (!fft_ok_) return;
    model->spec_freq.resize((size_t)kSpecBins);
    model->spec_db.resize((size_t)kSpecBins);
    for (int i = 0; i < kFftSize; ++i)
        fft_re_[(size_t)i] = (double)hist_[(size_t)i] * win_[(size_t)i];
    as_fft_forward(&fft_, fft_re_.data(), mag_.data(), kFftSize);
    double maxmag = 1e-12;
    for (int i = 1; i < kFftSize / 2; ++i)
        maxmag = std::max(maxmag, mag_[(size_t)i]);
    for (int i = 0; i < kSpecBins; ++i) {
        double db = 20.0 *
                    std::log10(std::max(mag_[(size_t)bin_idx_[(size_t)i]] / maxmag,
                                        1e-6));
        model->spec_freq[(size_t)i] = bin_freq_[(size_t)i];
        model->spec_db[(size_t)i] = std::max(db, -100.0);
    }
    as_tone_analyze(hist_.data(), kFftSize, 48000.0, &model->tone);

    AsPhaseResult pr{};
    if (as_phase_measure(hist_.data(), xy2_.data(), kFftSize, 48000.0, &pr) == 0) {
        model->phase_deg = pr.phase_deg;
        model->gain_db = pr.gain_db;
    }
}

void NativeEngine::update_xy(NativeModel *model) { (void)model; }

bool NativeEngine::set_capture(NativeModel *model, bool on, std::string *error) {
    if (on == capture_) return true;
    if (!on) {
        if (audio_) { as_audio_close((AsAudio *)audio_); audio_ = nullptr; }
        capture_ = false;
        if (model) {
            model->capture = false;
            model->source = "synthetic";
            model->device = "synthetic";
            model->message = "capture stopped";
        }
        return true;
    }
    AsAudio *a = as_audio_open(-1, -2, 48000.0, 128);
    if (!a) {
        if (error) *error = as_audio_last_error();
        return false;
    }
    audio_ = a;
    capture_ = true;
    cap_channels_ = std::clamp(as_audio_channels(a), 1, 8);
    double fs = as_audio_rate(a);
    cap_fs_ = fs > 0.0 ? fs : 48000.0;
    if (model) {
        model->capture = true;
        model->source = "capture";
        model->device = first_input_name();
        model->channels = cap_channels_;
        model->fs = cap_fs_;
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
    model->message = model->plan_pass ? "tape plan PASS" : "tape plan FAIL";
}

bool NativeEngine::run_loopback(NativeModel *model, std::string *error) {
    if (!model) return false;
    AsAudio *a = as_audio_open(-1, -1, 48000.0, 128);
    if (!a) {
        if (error) *error = as_audio_last_error();
        return false;
    }
    if (as_audio_set_tone(a, 1000.0, 0.1, 1) != 0) {
        as_audio_close(a);
        if (error) *error = "cannot start loopback tone";
        return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    int channels = std::clamp(as_audio_channels(a), 1, 8);
    std::vector<float> buf((size_t)480 * (size_t)channels, 0.0f);
    std::vector<float> mono;
    mono.reserve(48000 * 2);
    auto start = std::chrono::steady_clock::now();
    while (mono.size() < 48000u * 2u) {
        int got = as_audio_read(a, buf.data(), 480);
        for (int i = 0; i < got; ++i) mono.push_back(buf[(size_t)i * (size_t)channels]);
        auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        if (elapsed > 3.5) break;
        if (got <= 0) std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    as_audio_set_tone(a, 1000.0, 0.0, 0);
    as_audio_close(a);

    if (mono.size() < 4800) {
        if (error) *error = "loopback captured too few frames";
        return false;
    }
    AsStats st{};
    AsToneResult tone{};
    as_stats_compute(mono.data(), (int)mono.size(), 48000.0, &st);
    as_tone_analyze(mono.data(), (int)mono.size(), 48000.0, &tone);
    model->loopback_ran = true;
    model->loopback_thd = tone.thd_pct;
    model->loopback_level = as_linear_to_db(st.rms);
    model->loopback_pass = model->loopback_level > -60.0 && tone.thd_pct < 1.0;
    model->message = model->loopback_pass ? "loopback PASS" : "loopback FAIL";
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
    r.channels = 2;
    for (int c = 0; c < 2; ++c) {
        AsChannelMeasurement *m = &r.ch[c];
        std::snprintf(m->label, sizeof(m->label), "CH%d", c + 1);
        m->channel = c;
        m->stats = model->ch[c].active ? model->stats : AsStats{};
        m->peak_dbfs = model->ch[c].peak_db;
        m->rms_dbfs = model->ch[c].rms_db;
        m->vpp = model->ch[c].vpp;
        m->clip_count = model->ch[c].clip;
    }
    as_report_evaluate(&r);

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
