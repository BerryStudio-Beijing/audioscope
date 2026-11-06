#include "app/alpha_app.hpp"

#include "core/api.h"
#include "core/fft.h"
#include "core/generator.h"
#include "core/report.h"
#include "core/stats.h"
#include "core/window.h"
#include "platform/audio_backend.h"
#include "ui/ascii_canvas.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace audioscope {
namespace {

constexpr int kSampleRate = 48000;
constexpr int kScopeFrames = 480;
constexpr int kFftSize = 4096;
constexpr int kChunkFrames = 480;

#ifdef _WIN32
void enable_ansi() {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(h, &mode)) {
        SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
}
#else
void enable_ansi() {}
#endif

void print_help() {
    std::printf(
        "%s %s — sound-card audio repair workstation (0.2-alpha)\n"
        "\n"
        "Usage:\n"
        "  audioscope-alpha version\n"
        "  audioscope-alpha devices\n"
        "  audioscope-alpha modes\n"
        "  audioscope-alpha monitor [seconds] [--mode M] [--frontend] [--vpp V] [--cal FILE] [--channel 0|1]\n"
        "  audioscope-alpha check [seconds] [--synthetic]\n"
        "  audioscope-alpha tone [hz] [amp] [sec] [--allow-loud]\n"
        "  audioscope-alpha scope\n"
        "  audioscope-alpha fft\n"
        "  audioscope-alpha units\n"
        "  audioscope-alpha safety\n"
        "  audioscope-alpha session <dir>\n"
        "  audioscope-alpha calibrate [dir] --device NAME --channel N --gain DB --pad DB --rate HZ\n"
        "                              --dbfs -20 --measured-vrms V --confidence measured|datasheet|assumed\n"
        "  audioscope-alpha calibs [dir]\n"
        "  audioscope-alpha measure [seconds] [--cal FILE] [--mode M] [--frontend] [--vpp V]\n"
        "                           [--channel 0|1] [--out DIR] [--repeat N]\n"
        "                           [--max-vpp V] [--min-rms V] [--max-clip N]\n"
        "  audioscope-alpha selftest\n"
        "  audioscope-alpha help\n"
        "\n"
        "Safety: software can warn/mute, but cannot protect the sound card from\n"
        "over-voltage. Use a protected front end for unknown or high-level signals.\n",
        as_brand_string(), as_version_string());
}

std::vector<std::string> make_args(int argc, char **argv) {
    std::vector<std::string> out;
    for (int i = 0; i < argc; ++i) out.emplace_back(argv[i] ? argv[i] : "");
    return out;
}

std::string opt_value(const std::vector<std::string> &args, const char *key,
                      const std::string &fallback) {
    for (size_t i = 1; i + 1 < args.size(); ++i) {
        if (args[i] == key) return args[i + 1];
    }
    return fallback;
}

bool has_flag(const std::vector<std::string> &args, const char *key) {
    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == key) return true;
    }
    return false;
}

double opt_num(const std::vector<std::string> &args, const char *key, double fallback) {
    std::string v = opt_value(args, key, "");
    if (v.empty()) return fallback;
    char *end = nullptr;
    double d = std::strtod(v.c_str(), &end);
    return end != v.c_str() ? d : fallback;
}

int opt_int(const std::vector<std::string> &args, const char *key, int fallback) {
    return (int)std::lround(opt_num(args, key, (double)fallback));
}

double positional_num(const std::vector<std::string> &args, double fallback) {
    if (args.size() > 2 && !args[2].empty() && args[2][0] != '-') {
        char *end = nullptr;
        double d = std::strtod(args[2].c_str(), &end);
        if (end != args[2].c_str()) return d;
    }
    return fallback;
}

AsInputMode mode_from_args(const std::vector<std::string> &args) {
    std::string id = opt_value(args, "--mode", "line-pro");
    int mode = as_input_mode_parse(id.c_str());
    return mode >= 0 ? (AsInputMode)mode : AS_MODE_LINE_PRO;
}

bool load_calibration(const std::string &path, AsCalibration *cal) {
    if (path.empty() || !cal) return false;
    return as_calibration_read_json(cal, path.c_str()) == AS_OK;
}

double crest_factor(const AsStats &st) {
    if (st.rms > 1e-12) return st.peak / st.rms;
    return std::sqrt(2.0);
}

double calibrated_vrms(const AsCalibration *cal, double peak_dbfs) {
    if (!cal || !(cal->fs_volts_rms > 0.0)) return 0.0;
    return as_calibration_volts_rms(cal, peak_dbfs);
}

std::string render_scope(const std::vector<float> &mono, double fs,
                         AsStats *stats, const AsCalibration *cal,
                         AsInputMode mode, int channel) {
    const int w = 78;
    const int h = 22;
    AsciiCanvas c(w, h);
    c.clear(' ');
    c.frame();
    char title[96];
    std::snprintf(title, sizeof(title), "AudioScope by BerryStudio  |  SCOPE  |  %s  |  CH%d",
                  as_input_mode_id(mode), channel + 1);
    c.text(2, 1, title);

    AsStats st;
    as_stats_compute(mono.data(), (int)mono.size(), fs, &st);
    if (stats) *stats = st;

    const int top = 3;
    const int bottom = h - 3;
    const int mid = (top + bottom) / 2;
    c.hline(1, w - 2, mid, '.');
    double peak = std::max(st.peak, 1e-9);
    int n = (int)mono.size();
    for (int x = 1; x < w - 1; ++x) {
        int i0 = (x - 1) * n / std::max(1, w - 2);
        int i1 = x * n / std::max(1, w - 2);
        if (i1 <= i0) i1 = i0 + 1;
        if (i1 > n) i1 = n;
        double lo = 1e9, hi = -1e9;
        for (int i = i0; i < i1; ++i) {
            double v = (double)mono[(size_t)i];
            lo = std::min(lo, v);
            hi = std::max(hi, v);
        }
        int y_hi = mid - (int)std::lround(hi / peak * (double)(bottom - top) / 2.0);
        int y_lo = mid - (int)std::lround(lo / peak * (double)(bottom - top) / 2.0);
        y_hi = std::clamp(y_hi, top, bottom);
        y_lo = std::clamp(y_lo, top, bottom);
        c.vline(x, std::min(y_hi, y_lo), std::max(y_hi, y_lo), '|');
    }

    char line[128];
    double peak_dbfs = as_linear_to_db(st.peak);
    double vrms = calibrated_vrms(cal, peak_dbfs);
    if (vrms > 0.0) {
        double vpp = 2.0 * crest_factor(st) * vrms;
        std::snprintf(line, sizeof(line), "Vpp %.3f V   RMS %.3f V   %6.1f Hz   %s",
                      vpp, vrms, st.freq, as_confidence_name(cal->confidence));
    } else {
        std::snprintf(line, sizeof(line), "peak %.1f dBFS   RMS %.1f dBFS   %6.1f Hz   assumed",
                      peak_dbfs, as_linear_to_db(st.rms), st.freq);
    }
    c.text(2, h - 2, line);
    return c.to_string();
}

std::string render_spectrum(const std::vector<double> &mag, double fs,
                            int peak_bin) {
    const int w = 78;
    const int h = 18;
    AsciiCanvas c(w, h);
    c.clear(' ');
    c.frame();
    c.text(2, 1, "AudioScope by BerryStudio  |  FFT  |  Hann 4096  |  log 20 Hz-20 kHz");

    int bins = (int)mag.size();
    double maxv = 1e-12;
    for (int i = 1; i < bins; ++i) maxv = std::max(maxv, mag[(size_t)i]);

    const int top = 3;
    const int bottom = h - 3;
    const double fmin = 20.0;
    const double fmax = std::min(20000.0, fs / 2.0);
    for (int x = 1; x < w - 1; ++x) {
        double t = (double)(x - 1) / (double)std::max(1, w - 2);
        double f = fmin * std::pow(fmax / fmin, t);
        int b = (int)std::lround(f * 4096.0 / fs);
        b = std::clamp(b, 1, bins - 2);
        double db = 20.0 * std::log10(std::max(mag[(size_t)b], 1e-12) / maxv);
        int bars = (int)std::lround((db + 80.0) / 80.0 * (double)(bottom - top));
        bars = std::clamp(bars, 0, bottom - top);
        for (int y = 0; y < bars; ++y) c.put(x, bottom - y, '#');
    }
    char line[128];
    double f = as_fft_bin_hz(peak_bin, 4096, fs);
    std::snprintf(line, sizeof(line), "peak %.1f Hz   bin %d   log axis", f, peak_bin);
    c.text(2, h - 2, line);
    return c.to_string();
}

int capture_seconds(AsAudio *audio, double seconds, int channel,
                    std::vector<float> *out, int *clip_count) {
    if (!audio || !out) return 0;
    out->clear();
    if (clip_count) *clip_count = 0;
    double fs = as_audio_rate(audio);
    if (!(fs > 0.0)) fs = kSampleRate;
    int target = (int)(seconds * fs);
    if (target <= 0) target = kScopeFrames;
    std::vector<float> buf((size_t)kChunkFrames * 2, 0.0f);
    auto start = std::chrono::steady_clock::now();
    while ((int)out->size() < target) {
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - start).count();
        if (elapsed > seconds + 3.0) break;
        int got = as_audio_read(audio, buf.data(), kChunkFrames);
        for (int i = 0; i < got; ++i) {
            float v = buf[(size_t)i * 2 + channel];
            out->push_back(v);
            if (clip_count && std::fabs(v) >= 0.999f) ++*clip_count;
        }
        if (got <= 0) std::this_thread::sleep_for(std::chrono::milliseconds(2));
        else std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
    return (int)out->size();
}

AsStatus precheck_guard(AsSafetyGuard *guard, AsInputMode mode,
                        const std::vector<std::string> &args) {
    int frontend = has_flag(args, "--frontend") ? 1 : 0;
    double vpp = opt_num(args, "--vpp", 1.0);
    char reason[192];
    AsStatus rc = as_safety_precheck(guard, (int)mode, vpp, frontend, reason, sizeof(reason));
    if (rc != AS_OK) {
        std::fprintf(stderr, "precheck blocked: %s\n", reason);
        return rc;
    }
    std::printf("precheck: %s\n", reason);
    return AS_OK;
}

/* ------------------------------------------------------------------ */
/* Self test                                                           */
/* ------------------------------------------------------------------ */

int run_self_test() {
    int failures = 0;
    auto check = [&](bool ok, const char *name) {
        std::printf("  %s  %s\n", ok ? "ok  " : "FAIL", name);
        if (!ok) ++failures;
    };

    check(std::strcmp(as_version_string(), AS_VERSION) == 0, "version string");
    check(std::strcmp(as_brand_string(), AS_BRAND) == 0, "brand string");

    double v = 0.0;
    check(as_units_convert(-20.0, AS_UNIT_DBFS, AS_UNIT_VRMS, 1.0,
                           std::sqrt(2.0), &v) == AS_OK &&
              std::fabs(v - 0.1) < 1e-9, "dBFS -> Vrms");
    check(as_units_convert(-20.0, AS_UNIT_DBFS, AS_UNIT_DBU, 1.0,
                           std::sqrt(2.0), &v) == AS_OK &&
              std::fabs(v + 17.78) < 0.01, "dBFS -> dBu");
    check(as_units_convert(-20.0, AS_UNIT_DBFS, AS_UNIT_VPP, 1.0,
                           std::sqrt(2.0), &v) == AS_OK &&
              std::fabs(v - 0.282842712) < 1e-6, "dBFS -> Vpp");

    check(as_input_mode_parse("inst") == AS_MODE_INST, "parse inst");
    check(as_input_mode_parse("line-consumer") == AS_MODE_LINE_CONSUMER,
          "parse line-consumer");
    check(as_input_mode_spec(AS_MODE_INST) != nullptr &&
              as_input_mode_spec(AS_MODE_INST)->input_impedance_ohm > 900000.0,
          "inst is high-Z");

    AsSafetyGuard *g = as_safety_create();
    char reason[160];
    check(g != nullptr, "safety create");
    check(as_safety_precheck(g, AS_MODE_INST, 1.0, 0, reason, sizeof(reason)) == AS_OK,
          "precheck low voltage without front end");
    check(as_safety_arm(g) == AS_OK, "arm");
    check(as_safety_start(g) == AS_OK, "start");
    int clip_ch = -1;
    double clip_peak = 0.0;
    float clip_frames[4] = {0.1f, -0.2f, 1.1f, 0.0f};
    check(as_safety_feed(g, clip_frames, 2, 2, &clip_ch, &clip_peak) == AS_OK &&
              as_safety_state(g) == AS_SAFETY_FAULT && clip_ch == 0,
          "clip feed latches");
    check(as_safety_reset(g) == AS_OK && as_safety_state(g) == AS_SAFETY_IDLE,
          "reset");
    check(as_safety_precheck(g, AS_MODE_PROBE, 10.0, 0, reason, sizeof(reason)) == AS_ERR_RANGE,
          "probe requires front end");
    char range[32];
    check(as_safety_recommend_range(1.0, 0, range, sizeof(range)) == AS_OK &&
              std::string(range).find("1 V") != std::string::npos,
          "recommend +/-1 V");
    as_safety_destroy(g);

    namespace fs = std::filesystem;
    fs::path tmp = fs::temp_directory_path() / "audioscope-v02-selftest";
    std::error_code ec;
    fs::remove_all(tmp, ec);
    fs::create_directories(tmp, ec);

    AsCalibration cal;
    as_calibration_init(&cal);
    std::snprintf(cal.device_id, sizeof(cal.device_id), "%s", "test-device");
    cal.fs_volts_rms = 1.0;
    cal.gain_db = 12.0;
    cal.pad_db = -6.0;
    cal.sample_rate = 48000;
    cal.confidence = AS_CONF_MEASURED;
    as_calibration_seal(&cal);
    std::string cal_path = (tmp / "cal.json").string();
    check(as_calibration_write_json(&cal, cal_path.c_str()) == AS_OK,
          "calibration write");
    AsCalibration loaded;
    check(as_calibration_read_json(&loaded, cal_path.c_str()) == AS_OK,
          "calibration read");
    check(loaded.pad_db == -6.0 && loaded.sample_rate == 48000,
          "calibration pad/rate");
    char err[96];
    check(as_calibration_validate(&loaded, err, sizeof(err)) == AS_OK,
          "calibration validate");
    check(std::fabs(as_calibration_volts_rms(&loaded, -20.0) - 0.1) < 1e-9,
          "calibration volts");

    AsSession *s = as_session_create(tmp.string().c_str(), "selftest");
    check(s != nullptr, "session create");
    check(as_session_event(s, "note", "{\"text\":\"hello\"}") == AS_OK,
          "session event");
    check(as_session_count(s) == 1, "session count");
    as_session_destroy(s);

    AsFft fft;
    check(as_fft_init(&fft, kFftSize) == 0, "fft init");
    std::vector<float> sig(kFftSize);
    std::vector<double> x(kFftSize), win(kFftSize), mag(kFftSize / 2 + 1);
    AsGenerator gen;
    as_generator_init(&gen, kSampleRate);
    as_generator_sine(&gen, 1000.0, 0.5, sig.data(), kFftSize);
    for (int i = 0; i < kFftSize; ++i) x[(size_t)i] = (double)sig[(size_t)i];
    as_window_hann(win.data(), kFftSize);
    for (int i = 0; i < kFftSize; ++i) x[(size_t)i] *= win[(size_t)i];
    as_fft_forward(&fft, x.data(), mag.data(), kFftSize);
    int peak = as_fft_peak_bin(mag.data(), kFftSize);
    check(std::abs(as_fft_bin_hz(peak, kFftSize, kSampleRate) - 1000.0) < 12.0,
          "fft 1 kHz peak");
    as_fft_free(&fft);

    AsStats st;
    as_stats_compute(sig.data(), kFftSize, kSampleRate, &st);
    AsReport rep;
    as_report_init(&rep, "test-device", "inst", kSampleRate, kFftSize, 0.085);
    rep.confidence = AS_CONF_MEASURED;
    rep.fs_volts_rms = 1.0;
    std::snprintf(rep.calibration_id, sizeof(rep.calibration_id), "%s", "test-cal");
    rep.channels = 1;
    std::snprintf(rep.ch[0].label, sizeof(rep.ch[0].label), "CH1");
    rep.ch[0].channel = 0;
    rep.ch[0].stats = st;
    rep.ch[0].peak_dbfs = as_linear_to_db(st.peak);
    rep.ch[0].rms_dbfs = as_linear_to_db(st.rms);
    rep.ch[0].vrms = as_calibration_volts_rms(&cal, rep.ch[0].peak_dbfs);
    rep.ch[0].vpp = 2.0 * crest_factor(st) * rep.ch[0].vrms;
    rep.ch[0].clip_count = 0;
    rep.limits.max_vpp = 10.0;
    check(as_report_evaluate(&rep) == AS_OK && rep.verdict == 0, "report pass");
    std::string json = (tmp / "report.json").string();
    std::string html = (tmp / "report.html").string();
    check(as_report_write_json(&rep, json.c_str()) == AS_OK, "report json");
    check(as_report_write_html(&rep, html.c_str()) == AS_OK, "report html");
    check(fs::exists(json) && fs::file_size(json) > 100, "report json exists");
    check(fs::exists(html) && fs::file_size(html) > 200, "report html exists");
    std::ifstream in(html);
    std::string html_text((std::istreambuf_iterator<char>(in)),
                          std::istreambuf_iterator<char>());
    check(html_text.find("AudioScope by BerryStudio") != std::string::npos,
          "report brand");
    rep.ch[0].clip_count = 4;
    check(as_report_evaluate(&rep) == AS_OK && rep.verdict == 2, "report clip fail");

    AsciiCanvas canvas(20, 10);
    canvas.frame();
    check(canvas.to_string().find('#') != std::string::npos, "ascii canvas");

    fs::remove_all(tmp, ec);
    std::printf("\n%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}

/* ------------------------------------------------------------------ */
/* Commands                                                            */
/* ------------------------------------------------------------------ */

int cmd_scope() {
    std::vector<float> sig(kScopeFrames);
    AsGenerator gen;
    as_generator_init(&gen, kSampleRate);
    as_generator_sine(&gen, 1000.0, 0.5, sig.data(), kScopeFrames);
    AsStats st;
    AsCalibration cal;
    as_calibration_init(&cal);
    cal.fs_volts_rms = 1.0;
    cal.confidence = AS_CONF_MEASURED;
    std::string frame = render_scope(sig, kSampleRate, &st, &cal, AS_MODE_INST, 0);
    std::printf("%s", frame.c_str());
    std::printf("Synthetic 1 kHz sine, 48 kHz, %.0f frames. Vpp %.3f V, %.1f Hz\n",
                (double)kScopeFrames, st.vpp, st.freq);
    return 0;
}

int cmd_fft() {
    std::vector<float> sig(kFftSize);
    std::vector<double> x(kFftSize), win(kFftSize), mag(kFftSize / 2 + 1);
    AsGenerator gen;
    as_generator_init(&gen, kSampleRate);
    as_generator_sine(&gen, 1000.0, 0.5, sig.data(), kFftSize);
    for (int i = 0; i < kFftSize; ++i) x[(size_t)i] = (double)sig[(size_t)i];
    as_window_hann(win.data(), kFftSize);
    for (int i = 0; i < kFftSize; ++i) x[(size_t)i] *= win[(size_t)i];

    AsFft fft;
    if (as_fft_init(&fft, kFftSize) != 0) {
        std::fprintf(stderr, "fft init failed\n");
        return 1;
    }
    as_fft_forward(&fft, x.data(), mag.data(), kFftSize);
    int peak = as_fft_peak_bin(mag.data(), kFftSize);
    std::string frame = render_spectrum(mag, kSampleRate, peak);
    std::printf("%s", frame.c_str());
    std::printf("Synthetic 1 kHz sine, Hann 4096, peak bin %d = %.1f Hz\n",
                peak, as_fft_bin_hz(peak, kFftSize, kSampleRate));
    as_fft_free(&fft);
    return 0;
}

int cmd_devices() {
    AsDeviceInfo devs[64];
    int n = as_platform_list_devices(devs, 64);
    std::printf("platform: %s\n", as_platform_name());
    if (n == 0) {
        std::printf("no devices reported (alpha backend may be a stub)\n");
        return 0;
    }
    for (int i = 0; i < n; ++i) {
        std::printf("%2d  %-40s  %s%s  ch=%d  %.0f Hz\n", i, devs[i].name,
                    devs[i].is_input ? "IN " : "", devs[i].is_output ? "OUT" : "",
                    devs[i].channels, devs[i].sample_rate);
    }
    return 0;
}

int cmd_modes() {
    std::printf("%-16s %-24s %10s %10s %12s\n", "id", "label", "nom dBu", "max Vpp", "Z ohm");
    for (int i = 0; i < AS_MODE_COUNT; ++i) {
        const AsInputModeSpec *m = as_input_mode_spec((AsInputMode)i);
        std::printf("%-16s %-24s %10.2f %10.2f %12.0f%s\n", m->id, m->label,
                    m->nominal_dbu, m->max_vpp, m->input_impedance_ohm,
                    m->frontend_required ? "  [FE]" : "");
    }
    return 0;
}

int cmd_monitor(const std::vector<std::string> &args) {
    double seconds = positional_num(args, 5.0);
    if (seconds <= 0.0) seconds = 5.0;
    if (seconds > 60.0) seconds = 60.0;
    AsInputMode mode = mode_from_args(args);
    int channel = opt_int(args, "--channel", 0);
    if (channel < 0 || channel > 1) channel = 0;
    AsCalibration cal;
    bool have_cal = load_calibration(opt_value(args, "--cal", ""), &cal);

    AsSafetyGuard *guard = as_safety_create();
    if (precheck_guard(guard, mode, args) != AS_OK) {
        as_safety_destroy(guard);
        return 1;
    }
    if (as_safety_arm(guard) != AS_OK || as_safety_start(guard) != AS_OK) {
        std::fprintf(stderr, "safety start failed\n");
        as_safety_destroy(guard);
        return 1;
    }

    AsAudio *audio = as_audio_open(-1, -2, kSampleRate, 128);
    if (!audio) {
        std::fprintf(stderr, "audio open failed: %s\n", as_audio_last_error());
        as_safety_destroy(guard);
        return 1;
    }
    std::printf("monitoring input for %.1f s; mode %s; Ctrl-C to stop\n",
                seconds, as_input_mode_id(mode));
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    std::vector<float> buf((size_t)kChunkFrames * 2, 0.0f);
    std::vector<float> history((size_t)kScopeFrames, 0.0f);
    auto start = std::chrono::steady_clock::now();
    int frame_no = 0;
    enable_ansi();
    while (true) {
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - start).count();
        if (elapsed >= seconds) break;

        int got = as_audio_read(audio, buf.data(), kChunkFrames);
        if (got > 0) {
            int clip_ch = -1;
            double clip_peak = 0.0;
            if (as_safety_feed(guard, buf.data(), got, 2, &clip_ch, &clip_peak) != AS_OK ||
                as_safety_state(guard) == AS_SAFETY_FAULT) {
                std::printf("\x1b[2J\x1b[H");
                std::printf("FAULT: %s\n", as_safety_reason(guard));
                break;
            }
            history.erase(history.begin(), history.begin() + std::min(got, (int)history.size()));
            for (int i = 0; i < got && (int)history.size() < kScopeFrames; ++i)
                history.push_back(buf[(size_t)i * 2 + channel]);
        }
        AsStats st;
        std::string frame = render_scope(history, as_audio_rate(audio), &st,
                                         have_cal ? &cal : nullptr, mode, channel);
        std::printf("\x1b[2J\x1b[H");
        std::printf("%s", frame.c_str());
        std::printf("frame %d  %.1f s  |  %s  |  alpha: no hardware protection\n",
                    frame_no++, elapsed, as_safety_state_name(as_safety_state(guard)));
        std::fflush(stdout);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    as_audio_close(audio);
    as_safety_stop(guard);
    as_safety_destroy(guard);
    return 0;
}

int cmd_tone(const std::vector<std::string> &args) {
    double freq = positional_num(args, 1000.0);
    double amp = opt_num(args, "--amp", 0.1);
    double seconds = opt_num(args, "--seconds", 3.0);
    bool allow_loud = has_flag(args, "--allow-loud");
    if (freq < 20.0 || freq > 20000.0) {
        std::fprintf(stderr, "frequency must be 20..20000 Hz\n");
        return 1;
    }
    if (amp <= 0.0) amp = 0.1;
    if (amp > 0.1 && !allow_loud) {
        std::printf("output limited to 0.1 (-20 dBFS); use --allow-loud to override\n");
        amp = 0.1;
    }
    if (amp > 0.5) amp = 0.5;
    if (seconds <= 0.0 || seconds > 60.0) seconds = 3.0;
    AsAudio *audio = as_audio_open(-2, -1, kSampleRate, 128);
    if (!audio) {
        std::fprintf(stderr, "audio open failed: %s\n", as_audio_last_error());
        return 1;
    }
    if (as_audio_set_tone(audio, freq, amp, 1) != 0) {
        std::fprintf(stderr, "tone configuration failed\n");
        as_audio_close(audio);
        return 1;
    }
    std::printf("tone %.1f Hz, amp %.2f, %.1f s — use a protected front end\n",
                freq, amp, seconds);
    std::this_thread::sleep_for(std::chrono::milliseconds((int)(seconds * 1000.0)));
    as_audio_set_tone(audio, freq, amp, 0);
    as_audio_close(audio);
    return 0;
}

int cmd_units() {
    const double fs = 1.0;
    const double crest = std::sqrt(2.0);
    double vrms = 0, dbu = 0, dbv = 0, vpp = 0, dbfs = 0;
    as_units_convert(-20.0, AS_UNIT_DBFS, AS_UNIT_VRMS, fs, crest, &vrms);
    as_units_convert(-20.0, AS_UNIT_DBFS, AS_UNIT_DBU, fs, crest, &dbu);
    as_units_convert(-20.0, AS_UNIT_DBFS, AS_UNIT_DBV, fs, crest, &dbv);
    as_units_convert(-20.0, AS_UNIT_DBFS, AS_UNIT_VPP, fs, crest, &vpp);
    as_units_convert(vrms, AS_UNIT_VRMS, AS_UNIT_DBFS, fs, crest, &dbfs);
    std::printf("-20 dBFS @ 1.0 Vrms FS, sine:\n");
    std::printf("  %.9f Vrms\n  %.4f dBu\n  %.4f dBV\n  %.9f Vpp\n  back to %.6f dBFS\n",
                vrms, dbu, dbv, vpp, dbfs);
    return 0;
}

int cmd_safety() {
    AsSafetyGuard *g = as_safety_create();
    char reason[160];
    std::printf("initial: %s\n", as_safety_state_name(as_safety_state(g)));
    int rc = as_safety_precheck(g, AS_MODE_INST, 1.0, 0, reason, sizeof(reason));
    std::printf("precheck inst 1 Vpp no FE: %s (%s)\n",
                as_status_string((AsStatus)rc), reason);
    as_safety_arm(g);
    as_safety_start(g);
    std::printf("running: %s\n", as_safety_state_name(as_safety_state(g)));
    int clip_ch = -1;
    double clip_peak = 0.0;
    float frames[4] = {0.2f, -0.1f, 1.05f, 0.0f};
    as_safety_feed(g, frames, 2, 2, &clip_ch, &clip_peak);
    std::printf("after feed: %s (%s)\n", as_safety_state_name(as_safety_state(g)),
                as_safety_reason(g));
    as_safety_reset(g);
    rc = as_safety_precheck(g, AS_MODE_PROBE, 10.0, 0, reason, sizeof(reason));
    std::printf("precheck probe 10 Vpp no FE: %s (%s)\n",
                as_status_string((AsStatus)rc), reason);
    char range[32];
    as_safety_recommend_range(25.0, 1, range, sizeof(range));
    std::printf("25 Vpp with FE -> %s\n", range);
    as_safety_destroy(g);
    return 0;
}

int cmd_session(const std::string &dir) {
    AsSession *s = as_session_create(dir.c_str(), "v02-demo");
    if (!s) {
        std::fprintf(stderr, "session create failed\n");
        return 1;
    }
    as_session_event(s, "app_start", "{\"version\":\"0.2.0-alpha.1\"}");
    as_session_event(s, "input_mode", "{\"mode\":\"inst\"}");
    as_session_event(s, "note", "{\"text\":\"v0.2 session demo\"}");
    std::printf("session: %s (%d events)\n", as_session_path(s), as_session_count(s));
    as_session_destroy(s);
    return 0;
}

std::string sanitize(const std::string &s) {
    std::string out;
    for (char c : s) {
        if (std::isalnum((unsigned char)c) || c == '-' || c == '_') out.push_back(c);
        else out.push_back('_');
    }
    return out.empty() ? "default" : out;
}

int cmd_calibrate(const std::vector<std::string> &args) {
    std::string dir = "calibrations";
    if (args.size() > 2 && !args[2].empty() && args[2][0] != '-') dir = args[2];
    std::string device = opt_value(args, "--device", "default");
    int channel = opt_int(args, "--channel", 0);
    double gain = opt_num(args, "--gain", 0.0);
    double pad = opt_num(args, "--pad", 0.0);
    int rate = opt_int(args, "--rate", 48000);
    double dbfs = opt_num(args, "--dbfs", -20.0);
    double measured = opt_num(args, "--measured-vrms", 0.0);
    double fs_volts = opt_num(args, "--fs-volts", 0.0);
    std::string method = opt_value(args, "--method", "dmm");
    std::string conf_s = opt_value(args, "--confidence", "measured");

    if (fs_volts <= 0.0) {
        if (measured > 0.0) fs_volts = measured / std::pow(10.0, dbfs / 20.0);
        else fs_volts = 1.0;
    }
    AsConfidence conf = AS_CONF_MEASURED;
    if (conf_s == "datasheet") conf = AS_CONF_DATASHEET;
    else if (conf_s == "assumed") conf = AS_CONF_ASSUMED;

    AsCalibration cal;
    as_calibration_init(&cal);
    std::snprintf(cal.device_id, sizeof(cal.device_id), "%s", device.c_str());
    cal.input_channel = channel;
    cal.gain_db = gain;
    cal.pad_db = pad;
    cal.sample_rate = rate;
    cal.fs_volts_rms = fs_volts;
    cal.confidence = conf;
    std::snprintf(cal.method, sizeof(cal.method), "%s", method.c_str());
    as_calibration_seal(&cal);

    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    char name[256];
    std::snprintf(name, sizeof(name), "%s-ch%d-g%.1f-p%.1f-%d.json",
                  sanitize(device).c_str(), channel, gain, pad, rate);
    std::filesystem::path path = std::filesystem::path(dir) / name;
    int rc = as_calibration_write_json(&cal, path.string().c_str());
    if (rc != AS_OK) {
        std::fprintf(stderr, "calibration write failed: %s\n",
                     as_status_string((AsStatus)rc));
        return 1;
    }
    std::printf("calibration written: %s\n", path.string().c_str());
    std::printf("  FS %.6f Vrms · %s · ch%d · gain %.1f dB · pad %.1f dB · %d Hz\n",
                fs_volts, as_confidence_name(conf), channel, gain, pad, rate);
    return 0;
}

int cmd_calibs(const std::vector<std::string> &args) {
    std::string dir = "calibrations";
    if (args.size() > 2 && !args[2].empty() && args[2][0] != '-') dir = args[2];
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec)) {
        std::printf("no calibration directory: %s\n", dir.c_str());
        return 0;
    }
    int n = 0;
    for (const auto &entry : std::filesystem::directory_iterator(dir, ec)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;
        AsCalibration cal;
        if (as_calibration_read_json(&cal, entry.path().string().c_str()) != AS_OK) {
            std::printf("  [invalid] %s\n", entry.path().filename().string().c_str());
            continue;
        }
        std::printf("  %-36s ch%d gain %.1f pad %.1f %d Hz  FS %.4f Vrms  %s\n",
                    cal.device_id, cal.input_channel, cal.gain_db, cal.pad_db,
                    cal.sample_rate, cal.fs_volts_rms,
                    as_confidence_name(cal.confidence));
        ++n;
    }
    std::printf("%d calibration profile(s) in %s\n", n, dir.c_str());
    return 0;
}

int cmd_check(const std::vector<std::string> &args) {
    double seconds = positional_num(args, 1.0);
    bool synthetic = has_flag(args, "--synthetic");
    std::vector<float> frame;
    int clip_count = 0;
    AsAudio *audio = nullptr;
    if (!synthetic) {
        audio = as_audio_open(-1, -2, kSampleRate, 128);
        if (!audio) {
            std::printf("audio open failed: %s; using synthetic input\n",
                        as_audio_last_error());
            synthetic = true;
        }
    }
    if (synthetic) {
        frame.resize((size_t)(seconds * kSampleRate));
        AsGenerator gen;
        as_generator_init(&gen, kSampleRate);
        as_generator_sine(&gen, 1000.0, 0.05, frame.data(), (int)frame.size());
    } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        capture_seconds(audio, seconds, 0, &frame, &clip_count);
    }

    AsStats st;
    as_stats_compute(frame.data(), (int)frame.size(), kSampleRate, &st);
    double peak_dbfs = as_linear_to_db(st.peak);
    double rms_dbfs = as_linear_to_db(st.rms);
    double crest = crest_factor(st);

    std::vector<double> x(kFftSize, 0.0), win(kFftSize), mag(kFftSize / 2 + 1);
    for (int i = 0; i < kFftSize && i < (int)frame.size(); ++i)
        x[(size_t)i] = (double)frame[(size_t)i];
    as_window_hann(win.data(), kFftSize);
    for (int i = 0; i < kFftSize; ++i) x[(size_t)i] *= win[(size_t)i];
    AsFft fft;
    double hum_db = -120.0, tilt_db = 0.0;
    if (as_fft_init(&fft, kFftSize) == 0) {
        as_fft_forward(&fft, x.data(), mag.data(), kFftSize);
        int hum_bin = (int)std::lround(50.0 * kFftSize / kSampleRate);
        double max_mag = 1e-12;
        for (int i = 1; i < kFftSize / 2; ++i) max_mag = std::max(max_mag, mag[(size_t)i]);
        if (hum_bin > 1) hum_db = 20.0 * std::log10(std::max(mag[(size_t)hum_bin], 1e-12) / max_mag);
        double low = 0.0, high = 0.0;
        for (int i = 1; i < kFftSize / 2; ++i) {
            double f = as_fft_bin_hz(i, kFftSize, kSampleRate);
            if (f >= 200.0 && f <= 1000.0) low += mag[(size_t)i] * mag[(size_t)i];
            if (f >= 2000.0 && f <= 8000.0) high += mag[(size_t)i] * mag[(size_t)i];
        }
        if (low > 0.0) tilt_db = 10.0 * std::log10(std::max(high, 1e-12) / low);
        as_fft_free(&fft);
    }

    std::printf("signal check (%.1f s, %d frames)\n", seconds, (int)frame.size());
    std::printf("  peak %.1f dBFS   RMS %.1f dBFS   crest %.1f dB   %.1f Hz\n",
                peak_dbfs, rms_dbfs, 20.0 * std::log10(std::max(crest, 1e-6)), st.freq);
    std::printf("  50 Hz hum %.1f dB   HF tilt %.1f dB   clip %d\n",
                hum_db, tilt_db, clip_count);

    const char *suggest = "Line Pro";
    const char *why = "level is moderate; start with Line and low gain";
    if (peak_dbfs > -6.0) {
        suggest = "Line Pro";
        why = "hot signal; lower the source or use a pad before the interface";
    } else if (crest > 10.0 && tilt_db < -6.0) {
        suggest = "Instrument";
        why = "high crest factor and HF rolloff look like a passive pickup";
    } else if (crest < 4.0) {
        suggest = "Line Pro";
        why = "low crest factor looks like a line-level or compressed source";
    } else if (peak_dbfs < -40.0) {
        suggest = "Mic";
        why = "very low level; verify phantom is off and gain is up";
    }
    std::printf("\n  suggestion: %s (confidence: medium)\n  reason: %s\n",
                suggest, why);
    std::printf("  this is advisory only; the input mode is never switched automatically.\n");
    if (audio) as_audio_close(audio);
    return 0;
}

int cmd_measure(const std::vector<std::string> &args) {
    double seconds = positional_num(args, 2.0);
    if (seconds < 0.2) seconds = 0.2;
    if (seconds > 30.0) seconds = 30.0;
    AsInputMode mode = mode_from_args(args);
    int channel = opt_int(args, "--channel", 0);
    if (channel < 0 || channel > 1) channel = 0;
    int repeat = opt_int(args, "--repeat", 1);
    if (repeat < 1) repeat = 1;
    if (repeat > 5) repeat = 5;
    std::string out_dir = opt_value(args, "--out", "reports");
    std::string device = opt_value(args, "--device", "default");
    AsCalibration cal;
    bool have_cal = load_calibration(opt_value(args, "--cal", ""), &cal);

    AsSafetyGuard *guard = as_safety_create();
    if (precheck_guard(guard, mode, args) != AS_OK) {
        as_safety_destroy(guard);
        return 1;
    }
    if (as_safety_arm(guard) != AS_OK || as_safety_start(guard) != AS_OK) {
        std::fprintf(stderr, "safety start failed\n");
        as_safety_destroy(guard);
        return 1;
    }
    AsAudio *audio = as_audio_open(-1, -2, kSampleRate, 128);
    if (!audio) {
        std::fprintf(stderr, "audio open failed: %s\n", as_audio_last_error());
        as_safety_destroy(guard);
        return 1;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    std::vector<float> frame;
    int clip_count = 0;
    capture_seconds(audio, seconds, channel, &frame, &clip_count);
    AsStats st;
    as_stats_compute(frame.data(), (int)frame.size(), as_audio_rate(audio), &st);

    std::vector<double> vpps, freqs;
    for (int r = 0; r < repeat; ++r) {
        if (r > 0) {
            frame.clear();
            clip_count = 0;
            capture_seconds(audio, seconds, channel, &frame, &clip_count);
            as_stats_compute(frame.data(), (int)frame.size(), as_audio_rate(audio), &st);
        }
        double peak_dbfs = as_linear_to_db(st.peak);
        double vrms = calibrated_vrms(have_cal ? &cal : nullptr, peak_dbfs);
        double vpp = vrms > 0.0 ? 2.0 * crest_factor(st) * vrms : 0.0;
        vpps.push_back(vpp);
        freqs.push_back(st.freq);
    }
    double vpp_mean = 0.0, freq_mean = 0.0;
    for (double x : vpps) vpp_mean += x;
    for (double x : freqs) freq_mean += x;
    vpp_mean /= (double)vpps.size();
    freq_mean /= (double)freqs.size();
    double vpp_var = 0.0, freq_var = 0.0;
    for (size_t i = 0; i < vpps.size(); ++i) {
        vpp_var += (vpps[i] - vpp_mean) * (vpps[i] - vpp_mean);
        freq_var += (freqs[i] - freq_mean) * (freqs[i] - freq_mean);
    }
    if (vpps.size() > 1) {
        vpp_var /= (double)(vpps.size() - 1);
        freq_var /= (double)(freqs.size() - 1);
    } else {
        vpp_var = freq_var = 0.0;
    }

    AsReport rep;
    as_report_init(&rep, device.c_str(), as_input_mode_id(mode),
                   as_audio_rate(audio), (int)frame.size(), seconds);
    rep.confidence = have_cal ? cal.confidence : AS_CONF_ASSUMED;
    rep.fs_volts_rms = have_cal ? cal.fs_volts_rms : 0.0;
    std::snprintf(rep.calibration_id, sizeof(rep.calibration_id), "%s",
                  have_cal ? cal.device_id : "none");
    rep.channels = 1;
    rep.ch[0].channel = channel;
    std::snprintf(rep.ch[0].label, sizeof(rep.ch[0].label), "CH%d", channel + 1);
    rep.ch[0].stats = st;
    rep.ch[0].peak_dbfs = as_linear_to_db(st.peak);
    rep.ch[0].rms_dbfs = as_linear_to_db(st.rms);
    rep.ch[0].vrms = calibrated_vrms(have_cal ? &cal : nullptr, rep.ch[0].peak_dbfs);
    rep.ch[0].vpp = rep.ch[0].vrms > 0.0 ? 2.0 * crest_factor(st) * rep.ch[0].vrms : 0.0;
    rep.ch[0].clip_count = clip_count;
    rep.repeat_count = repeat;
    rep.vpp_std = std::sqrt(vpp_var);
    rep.freq_std = std::sqrt(freq_var);
    rep.limits.max_vpp = opt_num(args, "--max-vpp", 0.0);
    rep.limits.min_rms = opt_num(args, "--min-rms", 0.0);
    rep.limits.max_clip_samples = opt_int(args, "--max-clip", 0);
    rep.limits.min_freq_hz = opt_num(args, "--min-freq", 20.0);
    rep.limits.max_freq_hz = opt_num(args, "--max-freq", 20000.0);
    as_report_evaluate(&rep);

    std::error_code ec;
    std::filesystem::create_directories(out_dir, ec);
    std::string json = out_dir + "/" + rep.run_id + ".json";
    std::string html = out_dir + "/" + rep.run_id + ".html";
    int rc_json = as_report_write_json(&rep, json.c_str());
    int rc_html = as_report_write_html(&rep, html.c_str());

    AsSession *session = as_session_create(out_dir.c_str(), rep.run_id);
    if (session) {
        char event[128];
        std::snprintf(event, sizeof(event),
                      "{\"verdict\":\"%s\",\"mode\":\"measure\"}",
                      as_verdict_name(rep.verdict));
        as_session_event(session, "run_end", event);
        as_session_destroy(session);
    }

    std::printf("measure: %s · %s · %s\n", as_input_mode_id(mode),
                have_cal ? as_confidence_name(cal.confidence) : "assumed", rep.verdict_text);
    std::printf("  peak %.1f dBFS   RMS %.1f dBFS   %.1f Hz   clip %d\n",
                rep.ch[0].peak_dbfs, rep.ch[0].rms_dbfs, st.freq, clip_count);
    if (rep.ch[0].vrms > 0.0)
        std::printf("  Vpp %.4f V   Vrms %.4f V   repeat %d · Vpp σ %.4f\n",
                    rep.ch[0].vpp, rep.ch[0].vrms, repeat, rep.vpp_std);
    else
        std::printf("  voltage unavailable: no calibration (dBFS only)\n");
    if (rc_json == AS_OK) std::printf("  JSON: %s\n", json.c_str());
    if (rc_html == AS_OK) std::printf("  HTML: %s\n", html.c_str());

    as_audio_close(audio);
    as_safety_stop(guard);
    as_safety_destroy(guard);
    return rep.verdict == 2 ? 1 : 0;
}

} // namespace

int run_alpha_cli(int argc, char **argv) {
    enable_ansi();
    std::vector<std::string> args = make_args(argc, argv);
    if (args.size() < 2) {
        print_help();
        return 0;
    }
    const std::string &cmd = args[1];
    if (cmd == "version" || cmd == "--version" || cmd == "-v") {
        std::printf("%s %s\n", as_brand_string(), as_version_string());
        std::printf("platform: %s\n", as_platform_name());
        return 0;
    }
    if (cmd == "help" || cmd == "--help" || cmd == "-h") {
        print_help();
        return 0;
    }
    if (cmd == "selftest") return run_self_test();
    if (cmd == "devices") return cmd_devices();
    if (cmd == "modes") return cmd_modes();
    if (cmd == "scope") return cmd_scope();
    if (cmd == "fft") return cmd_fft();
    if (cmd == "units") return cmd_units();
    if (cmd == "safety") return cmd_safety();
    if (cmd == "monitor") return cmd_monitor(args);
    if (cmd == "check") return cmd_check(args);
    if (cmd == "tone") return cmd_tone(args);
    if (cmd == "session") return cmd_session(args.size() > 2 ? args[2] : "build/session");
    if (cmd == "calibrate") return cmd_calibrate(args);
    if (cmd == "calibs") return cmd_calibs(args);
    if (cmd == "measure") return cmd_measure(args);
    std::fprintf(stderr, "unknown command: %s\n\n", cmd.c_str());
    print_help();
    return 2;
}

} // namespace audioscope
