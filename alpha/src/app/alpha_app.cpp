#include "app/alpha_app.hpp"

#include "core/api.h"
#include "core/fft.h"
#include "core/generator.h"
#include "core/pdf.h"
#include "core/phase.h"
#include "core/repair.h"
#include "core/report.h"
#include "core/spectrogram.h"
#include "core/tone.h"
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
#include <ctime>
#include <iomanip>
#include <sstream>
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
        "%s %s — sound-card audio repair workstation (0.7-alpha)\n"
        "\n"
        "Usage:\n"
        "  audioscope-alpha version\n"
        "  audioscope-alpha devices\n"
        "  audioscope-alpha modes\n"
        "  audioscope-alpha monitor [seconds] [--mode M] [--frontend] [--vpp V] [--cal FILE] [--channel N]\n"
        "  audioscope-alpha check [seconds] [--synthetic]\n"
        "  audioscope-alpha xy [seconds] [--simulate --phase DEG --gain DB --drift PPM] [--ch1 N --ch2 N]\n"
        "  audioscope-alpha drift [seconds] [--simulate PPM] [--live]\n"
        "  audioscope-alpha spectrogram [seconds] [--synthetic] [--channel N]\n"
        "  audioscope-alpha wow [seconds] [--synthetic --wow PCT --flutter PCT]\n"
        "  audioscope-alpha crosstalk [seconds] [--synthetic]\n"
        "  audioscope-alpha impedance --r R --vsrc V --vdut V | --simulate Z\n"
        "  audioscope-alpha azimuth [seconds] [--synthetic --phase DEG]\n"
        "  audioscope-alpha compare <before.json> <after.json>\n"
        "  audioscope-alpha thd [seconds] [--synthetic --freq HZ --level DBFS --thd PCT]\n"
        "  audioscope-alpha imd [seconds] [--synthetic --standard smpte|din]\n"
        "  audioscope-alpha sweep [--start DBFS --end DBFS --step DB --csv FILE]\n"
        "  audioscope-alpha freqsweep [--start HZ --end HZ --points N --csv FILE]\n"
        "  audioscope-alpha templates\n"
        "  audioscope-alpha repair --template quick|pedal|tape|mixer|headphone [--out DIR]\n"
        "  audioscope-alpha ui [--web-src DIR] [--out DIR]\n"
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
        "                           [--channel N] [--channels all|0,1] [--out DIR] [--repeat N]\n"
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

int capture_interleaved(AsAudio *audio, double seconds,
                        std::vector<float> *out, std::vector<int> *clips) {
    if (!audio || !out) return 0;
    int channels = as_audio_channels(audio);
    if (channels < 1) channels = 2;
    if (channels > 8) channels = 8;
    out->clear();
    if (clips) clips->assign((size_t)channels, 0);
    double fs = as_audio_rate(audio);
    if (!(fs > 0.0)) fs = kSampleRate;
    int target_frames = (int)(seconds * fs);
    if (target_frames <= 0) target_frames = kScopeFrames;
    std::vector<float> buf((size_t)kChunkFrames * (size_t)channels, 0.0f);
    int frames = 0;
    auto start = std::chrono::steady_clock::now();
    while (frames < target_frames) {
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - start).count();
        if (elapsed > seconds + 3.0) break;
        int got = as_audio_read(audio, buf.data(), kChunkFrames);
        for (int i = 0; i < got; ++i) {
            for (int c = 0; c < channels; ++c) {
                float v = buf[(size_t)i * (size_t)channels + (size_t)c];
                out->push_back(v);
                if (clips && std::fabs(v) >= 0.999f) ++(*clips)[(size_t)c];
            }
        }
        frames += got;
        if (got <= 0) std::this_thread::sleep_for(std::chrono::milliseconds(2));
        else std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
    return frames;
}

void extract_channel(const std::vector<float> &interleaved, int channels,
                     int channel, std::vector<float> *mono) {
    if (!mono) return;
    mono->clear();
    if (channels < 1 || channel < 0 || channel >= channels) return;
    mono->reserve(interleaved.size() / (size_t)channels);
    for (size_t i = 0; i + (size_t)channels <= interleaved.size();
         i += (size_t)channels) {
        mono->push_back(interleaved[i + (size_t)channel]);
    }
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

    const int pn = 8192;
    std::vector<float> pa(pn), pb(pn);
    for (int i = 0; i < pn; ++i) {
        double t = (double)i / kSampleRate;
        pa[(size_t)i] = (float)(0.5 * std::sin(2.0 * 3.14159265358979323846 * 1000.0 * t));
        pb[(size_t)i] = (float)(0.5 * std::pow(10.0, 3.0 / 20.0) *
            std::sin(2.0 * 3.14159265358979323846 * 1000.0 * t +
                     45.0 * 3.14159265358979323846 / 180.0));
    }
    AsPhaseResult pr;
    check(as_phase_measure(pa.data(), pb.data(), pn, kSampleRate, &pr) == 0 &&
              std::fabs(pr.phase_deg - 45.0) < 2.0 &&
              std::fabs(pr.gain_db - 3.0) < 0.2,
          "phase/gain 45 deg +3 dB");

    const int wn = 10 * kSampleRate;
    std::vector<float> wow_sig(wn);
    double wow_phase = 0.0;
    for (int i = 0; i < wn; ++i) {
        double t = (double)i / kSampleRate;
        double f = 3000.0 * (1.0 + 0.003 * std::sin(2.0 * 3.14159265358979323846 * 0.5 * t)
                                + 0.001 * std::sin(2.0 * 3.14159265358979323846 * 8.0 * t));
        wow_phase += 2.0 * 3.14159265358979323846 * f / kSampleRate;
        wow_sig[(size_t)i] = (float)(0.5 * std::sin(wow_phase));
    }
    AsWowResult wr;
    check(as_wow_measure(wow_sig.data(), wn, kSampleRate, &wr) == 0 &&
              wr.wow_pct > 0.15 && wr.wow_pct < 0.5 &&
              wr.flutter_pct > 0.02 && wr.flutter_pct < 0.3,
          "wow/flutter 0.3% + 0.1%");

    std::vector<float> xt_a(4800), xt_b(4800);
    for (int i = 0; i < 4800; ++i) {
        double t = (double)i / kSampleRate;
        xt_a[(size_t)i] = (float)(0.1 * std::sin(2.0 * 3.14159265358979323846 * 1000.0 * t));
        xt_b[(size_t)i] = xt_a[(size_t)i] * 0.001f;
    }
    double xt_db = 0.0;
    check(as_crosstalk_db(xt_a.data(), xt_b.data(), 4800, &xt_db) == 0 &&
              std::fabs(xt_db + 60.0) < 0.5,
          "crosstalk -60 dB");

    double z = 0.0;
    check(as_impedance(1000.0, 0.5, 1.0, &z) == 0 && std::fabs(z - 1000.0) < 0.01,
          "impedance 1000 ohm");

    std::vector<float> tone_sig(8192);
    for (int i = 0; i < 8192; ++i) {
        double t = (double)i / kSampleRate;
        double v = 0.5 * std::sin(2.0 * 3.14159265358979323846 * 1000.0 * t)
                 + 0.5 * 0.0008 * std::sin(2.0 * 3.14159265358979323846 * 2000.0 * t)
                 + 0.5 * 0.0006 * std::sin(2.0 * 3.14159265358979323846 * 3000.0 * t);
        tone_sig[(size_t)i] = (float)v;
    }
    AsToneResult tr;
    check(as_tone_analyze(tone_sig.data(), 8192, kSampleRate, &tr) == 0 &&
              tr.thd_pct > 0.05 && tr.thd_pct < 0.2 && tr.snr_db > 40.0,
          "THD 0.1% / SNR");

    const int imdn = 32768;
    std::vector<float> imd_sig(imdn);
    for (int i = 0; i < imdn; ++i) {
        double t = (double)i / kSampleRate;
        double f1 = 60.0, f2 = 7000.0, side = 0.1 * 0.0005;
        double v = 0.4 * std::sin(2.0 * 3.14159265358979323846 * f1 * t)
                 + 0.1 * std::sin(2.0 * 3.14159265358979323846 * f2 * t)
                 + side * std::sin(2.0 * 3.14159265358979323846 * (f2 - f1) * t)
                 + side * std::sin(2.0 * 3.14159265358979323846 * (f2 + f1) * t)
                 + side * std::sin(2.0 * 3.14159265358979323846 * (f2 - 2.0 * f1) * t)
                 + side * std::sin(2.0 * 3.14159265358979323846 * (f2 + 2.0 * f1) * t);
        imd_sig[(size_t)i] = (float)v;
    }
    double imd_pct = 0.0, imd_f1 = 0.0, imd_f2 = 0.0;
    check(as_imd_analyze(imd_sig.data(), imdn, kSampleRate, 0,
                         &imd_pct, &imd_f1, &imd_f2) == 0 &&
              imd_pct > 0.05 && imd_pct < 0.25,
          "IMD SMPTE 0.1%");

    const int dn = 5 * kSampleRate;
    std::vector<float> da(dn), db(dn);
    for (int i = 0; i < dn; ++i) {
        double t = (double)i / kSampleRate;
        da[(size_t)i] = (float)(0.5 * std::sin(2.0 * 3.14159265358979323846 * 1000.0 * t));
        db[(size_t)i] = (float)(0.5 * std::sin(2.0 * 3.14159265358979323846 * 1000.1 * t));
    }
    double ppm = 0.0, drift_f = 0.0;
    check(as_drift_estimate(da.data(), db.data(), dn, kSampleRate, 4096,
                            &ppm, &drift_f) == 0 &&
              std::fabs(ppm - 100.0) < 10.0,
          "drift 100 ppm");

    AsSpectrogram spec;
    check(as_spectrogram_init(&spec, 16, 24, 20.0, 20000.0) == 0, "spectrogram init");
    std::vector<double> sx(2048), sw(2048), sm(1025);
    for (int i = 0; i < 2048; ++i)
        sx[(size_t)i] = 0.5 * std::sin(2.0 * 3.14159265358979323846 * 1000.0 *
                                      (double)i / kSampleRate);
    as_window_hann(sw.data(), 2048);
    for (int i = 0; i < 2048; ++i) sx[(size_t)i] *= sw[(size_t)i];
    AsFft sfft;
    check(as_fft_init(&sfft, 2048) == 0, "spectrogram fft");
    as_fft_forward(&sfft, sx.data(), sm.data(), 2048);
    as_spectrogram_push(&spec, sm.data(), 2048, kSampleRate);
    float spec_best = -120.0f;
    int spec_row = 0;
    for (int row = 0; row < spec.rows; ++row) {
        float v = as_spectrogram_get(&spec, 0, row);
        if (v > spec_best) { spec_best = v; spec_row = row; }
    }
    check(spec_best > -10.0f && spec_row >= 11 && spec_row <= 15, "spectrogram 1 kHz");
    as_fft_free(&sfft);
    as_spectrogram_free(&spec);

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
    std::string pdf = (tmp / "report.pdf").string();
    check(as_report_write_json(&rep, json.c_str()) == AS_OK, "report json");
    check(as_report_write_html(&rep, html.c_str()) == AS_OK, "report html");
    check(as_report_write_pdf(&rep, pdf.c_str()) == AS_OK, "report pdf");
    check(fs::exists(json) && fs::file_size(json) > 100, "report json exists");
    check(fs::exists(html) && fs::file_size(html) > 200, "report html exists");
    check(fs::exists(pdf) && fs::file_size(pdf) > 500, "report pdf exists");
    std::ifstream pdf_in(pdf, std::ios::binary);
    char pdf_head[9] = {0};
    pdf_in.read(pdf_head, 8);
    check(std::string(pdf_head).find("%PDF-1.4") == 0, "report pdf header");
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

std::string render_xy(const std::vector<float> &x, const std::vector<float> &y,
                      const AsPhaseResult &pr) {
    const int w = 60;
    const int h = 28;
    AsciiCanvas c(w, h);
    c.clear(' ');
    c.frame();
    c.text(2, 1, "AudioScope by BerryStudio  |  XY / Lissajous");
    int midx = w / 2, midy = h / 2;
    c.vline(midx, 3, h - 3, '.');
    c.hline(2, w - 3, midy, '.');
    double mx = 1e-9, my = 1e-9;
    for (float v : x) mx = std::max(mx, (double)std::fabs(v));
    for (float v : y) my = std::max(my, (double)std::fabs(v));
    size_t n = std::min(x.size(), y.size());
    size_t limit = std::min<size_t>(n, 8000);
    for (size_t i = 0; i < limit; ++i) {
        int px = midx + (int)std::lround((double)x[i] / mx * (double)(w / 2 - 3));
        int py = midy - (int)std::lround((double)y[i] / my * (double)(h / 2 - 4));
        if (px > 1 && px < w - 2 && py > 2 && py < h - 2) c.put(px, py, '*');
    }
    char line[128];
    std::snprintf(line, sizeof(line), "phase %.1f deg  gain %.2f dB  %s",
                  pr.phase_deg, pr.gain_db,
                  std::fabs(pr.phase_deg) > 135.0 ? "POLARITY INVERTED" : "polarity normal");
    c.text(2, h - 2, line);
    return c.to_string();
}

int cmd_xy(const std::vector<std::string> &args) {
    double seconds = positional_num(args, 2.0);
    if (seconds < 0.2) seconds = 0.2;
    if (seconds > 30.0) seconds = 30.0;
    bool simulate = has_flag(args, "--simulate");
    double phase_deg = opt_num(args, "--phase", 45.0);
    double gain_db = opt_num(args, "--gain", 0.0);
    double drift_ppm = opt_num(args, "--drift", 0.0);
    int ch1 = opt_int(args, "--ch1", 0);
    int ch2 = opt_int(args, "--ch2", 1);
    double fs = kSampleRate;
    std::vector<float> a, b;
    bool simulated = simulate;
    if (simulate) {
        int n = (int)(seconds * fs);
        a.resize((size_t)n);
        b.resize((size_t)n);
        double g = std::pow(10.0, gain_db / 20.0);
        double pr = phase_deg * 3.14159265358979323846 / 180.0;
        double f2 = 1000.0 * (1.0 + drift_ppm * 1e-6);
        for (int i = 0; i < n; ++i) {
            double t = (double)i / fs;
            a[(size_t)i] = (float)(0.5 * std::sin(2.0 * 3.14159265358979323846 * 1000.0 * t));
            b[(size_t)i] = (float)(0.5 * g * std::sin(2.0 * 3.14159265358979323846 * f2 * t + pr));
        }
    } else {
        AsAudio *audio = as_audio_open(-1, -2, kSampleRate, 128);
        if (!audio) {
            std::fprintf(stderr, "audio open failed: %s\n", as_audio_last_error());
            return 1;
        }
        int channels = as_audio_channels(audio);
        if (channels < 2) {
            std::fprintf(stderr, "XY needs at least 2 input channels; device reports %d\n", channels);
            as_audio_close(audio);
            return 1;
        }
        if (ch1 < 0 || ch1 >= channels) ch1 = 0;
        if (ch2 < 0 || ch2 >= channels) ch2 = 1;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        std::vector<float> interleaved;
        std::vector<int> clips;
        capture_interleaved(audio, seconds, &interleaved, &clips);
        fs = as_audio_rate(audio);
        extract_channel(interleaved, channels, ch1, &a);
        extract_channel(interleaved, channels, ch2, &b);
        as_audio_close(audio);
    }

    AsPhaseResult pr;
    if (as_phase_measure(a.data(), b.data(), (int)std::min(a.size(), b.size()), fs, &pr) != 0) {
        std::fprintf(stderr, "phase measurement failed (no stable fundamental)\n");
        return 1;
    }
    std::printf("%s\n", render_xy(a, b, pr).c_str());
    std::printf("CH%d vs CH%d: phase %.2f deg, gain %.2f dB, freq %.1f Hz\n",
                ch1 + 1, ch2 + 1, pr.phase_deg, pr.gain_db, pr.freq_hz);
    if (simulated) {
        std::printf("simulated phase %.1f deg, gain %.2f dB, drift %.1f ppm\n",
                    phase_deg, gain_db, drift_ppm);
    } else {
        std::printf("live same-device channels share a clock; drift is not observable here.\n");
    }
    if (std::fabs(pr.phase_deg) > 135.0)
        std::printf("POLARITY: inverted (or ~180 deg phase)\n");
    return 0;
}

int cmd_drift(const std::vector<std::string> &args) {
    double seconds = positional_num(args, 5.0);
    if (seconds < 1.0) seconds = 1.0;
    if (seconds > 30.0) seconds = 30.0;
    bool live = has_flag(args, "--live");
    double ppm_in = opt_num(args, "--simulate", 100.0);
    double freq = opt_num(args, "--freq", 1000.0);
    double fs = kSampleRate;
    std::vector<float> a, b;
    if (live) {
        AsAudio *audio = as_audio_open(-1, -2, kSampleRate, 128);
        if (!audio) {
            std::fprintf(stderr, "audio open failed: %s\n", as_audio_last_error());
            return 1;
        }
        int channels = as_audio_channels(audio);
        if (channels < 2) {
            std::fprintf(stderr, "drift needs 2 channels; device reports %d\n", channels);
            as_audio_close(audio);
            return 1;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        std::vector<float> interleaved;
        std::vector<int> clips;
        capture_interleaved(audio, seconds, &interleaved, &clips);
        fs = as_audio_rate(audio);
        extract_channel(interleaved, channels, 0, &a);
        extract_channel(interleaved, channels, 1, &b);
        as_audio_close(audio);
        std::printf("live same-device capture: CH1/CH2 share a clock; ppm is not observable.\n");
        std::printf("Use two devices / an aggregate device, or --simulate N for the algorithm.\n");
        return 0;
    } else {
        int n = (int)(seconds * fs);
        a.resize((size_t)n);
        b.resize((size_t)n);
        double f2 = freq * (1.0 + ppm_in * 1e-6);
        for (int i = 0; i < n; ++i) {
            double t = (double)i / fs;
            a[(size_t)i] = (float)(0.5 * std::sin(2.0 * 3.14159265358979323846 * freq * t));
            b[(size_t)i] = (float)(0.5 * std::sin(2.0 * 3.14159265358979323846 * f2 * t));
        }
    }
    double ppm = 0.0, f = 0.0;
    if (as_drift_estimate(a.data(), b.data(), (int)std::min(a.size(), b.size()),
                          fs, 4096, &ppm, &f) != 0) {
        std::fprintf(stderr, "drift estimate failed (need more blocks)\n");
        return 1;
    }
    std::printf("drift estimate: %.2f ppm at %.1f Hz", ppm, f);
    if (!live) std::printf(" (injected %.2f ppm)", ppm_in);
    std::printf("\n");
    return 0;
}

std::string render_spectrogram(const AsSpectrogram &spec, double peak_hz) {
    const int w = 78;
    const int h = 24;
    AsciiCanvas c(w, h);
    c.clear(' ');
    c.frame();
    c.text(2, 1, "AudioScope by BerryStudio  |  SPECTROGRAM  |  20 Hz-20 kHz");
    const char *ramp = " .:-=+*#%@";
    const int plot_rows = h - 5;
    for (int yy = 0; yy < plot_rows; ++yy) {
        int row = yy * (spec.rows - 1) / std::max(1, plot_rows - 1);
        int y = h - 3 - yy;
        for (int age = 0; age < spec.count; ++age) {
            int x = 1 + age * (w - 2) / std::max(1, spec.count - 1);
            float db = as_spectrogram_get(&spec, age, row);
            int idx = (int)std::lround((std::clamp(db, -80.0f, 0.0f) + 80.0) / 80.0 * 9.0);
            c.put(x, y, ramp[std::clamp(idx, 0, 9)]);
        }
    }
    char line[128];
    std::snprintf(line, sizeof(line), "peak %.1f Hz   %d frames   -80..0 dB", peak_hz, spec.count);
    c.text(2, h - 2, line);
    return c.to_string();
}

int cmd_spectrogram(const std::vector<std::string> &args) {
    double seconds = positional_num(args, 3.0);
    if (seconds < 0.5) seconds = 0.5;
    if (seconds > 30.0) seconds = 30.0;
    bool synthetic = has_flag(args, "--synthetic");
    int channel = opt_int(args, "--channel", 0);
    std::vector<float> frame;
    double fs = kSampleRate;
    if (synthetic) {
        int n = (int)(seconds * fs);
        frame.resize((size_t)n);
        for (int i = 0; i < n; ++i) {
            double t = (double)i / fs;
            double v = 0.35 * std::sin(2.0 * 3.14159265358979323846 * 1000.0 * t)
                     + 0.15 * std::sin(2.0 * 3.14159265358979323846 * 3000.0 * t)
                     + 0.08 * std::sin(2.0 * 3.14159265358979323846 * 5000.0 * t);
            frame[(size_t)i] = (float)v;
        }
    } else {
        AsAudio *audio = as_audio_open(-1, -2, kSampleRate, 128);
        if (!audio) {
            std::fprintf(stderr, "audio open failed: %s\n", as_audio_last_error());
            return 1;
        }
        int channels = as_audio_channels(audio);
        if (channel < 0 || channel >= channels) channel = 0;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        std::vector<float> interleaved;
        std::vector<int> clips;
        capture_interleaved(audio, seconds, &interleaved, &clips);
        fs = as_audio_rate(audio);
        extract_channel(interleaved, channels, channel, &frame);
        as_audio_close(audio);
    }

    const int nfft = 2048;
    const int hop = 512;
    AsSpectrogram spec;
    if (as_spectrogram_init(&spec, 80, 96, 20.0, 20000.0) != 0) {
        std::fprintf(stderr, "spectrogram init failed\n");
        return 1;
    }
    AsFft fft;
    if (as_fft_init(&fft, nfft) != 0) {
        std::fprintf(stderr, "fft init failed\n");
        as_spectrogram_free(&spec);
        return 1;
    }
    std::vector<double> x((size_t)nfft), win((size_t)nfft), mag((size_t)nfft / 2 + 1);
    as_window_hann(win.data(), nfft);
    for (int start = 0; start + nfft <= (int)frame.size(); start += hop) {
        for (int i = 0; i < nfft; ++i) x[(size_t)i] = (double)frame[(size_t)(start + i)];
        for (int i = 0; i < nfft; ++i) x[(size_t)i] *= win[(size_t)i];
        as_fft_forward(&fft, x.data(), mag.data(), nfft);
        as_spectrogram_push(&spec, mag.data(), nfft, fs);
    }
    float best = -120.0f;
    int best_row = 0;
    for (int row = 0; row < spec.rows; ++row) {
        for (int age = 0; age < spec.count; ++age) {
            float db = as_spectrogram_get(&spec, age, row);
            if (db > best) { best = db; best_row = row; }
        }
    }
    double peak_hz = spec.fmin * std::pow(spec.fmax / spec.fmin,
        spec.rows > 1 ? (double)best_row / (double)(spec.rows - 1) : 0.0);
    std::printf("%s", render_spectrogram(spec, peak_hz).c_str());
    std::printf("peak row %d ≈ %.1f Hz · %.1f dB relative\n", best_row, peak_hz, best);
    as_fft_free(&fft);
    as_spectrogram_free(&spec);
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
        std::printf("%2d  %-36s  %s%s  in=%d out=%d  %.0f Hz\n", i, devs[i].name,
                    devs[i].is_input ? "IN " : "", devs[i].is_output ? "OUT" : "",
                    devs[i].in_channels, devs[i].out_channels, devs[i].sample_rate);
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
    int channels = as_audio_channels(audio);
    if (channels < 1) channels = 2;
    if (channel >= channels) channel = 0;
    std::printf("monitoring input for %.1f s; mode %s; %d ch; Ctrl-C to stop\n",
                seconds, as_input_mode_id(mode), channels);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    std::vector<float> buf((size_t)kChunkFrames * (size_t)channels, 0.0f);
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
            if (as_safety_feed(guard, buf.data(), got, channels, &clip_ch, &clip_peak) != AS_OK ||
                as_safety_state(guard) == AS_SAFETY_FAULT) {
                std::printf("\x1b[2J\x1b[H");
                std::printf("FAULT: %s\n", as_safety_reason(guard));
                break;
            }
            history.erase(history.begin(), history.begin() + std::min(got, (int)history.size()));
            for (int i = 0; i < got && (int)history.size() < kScopeFrames; ++i)
                history.push_back(buf[(size_t)i * (size_t)channels + (size_t)channel]);
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
    as_session_event(s, "app_start", "{\"version\":\"0.7.0-alpha.1\"}");
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
    std::vector<float> interleaved;
    std::vector<int> clips;
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
        int channels = as_audio_channels(audio);
        if (channels < 1) channels = 2;
        capture_interleaved(audio, seconds, &interleaved, &clips);
        extract_channel(interleaved, channels, 0, &frame);
        clip_count = clips.empty() ? 0 : clips[0];
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
    int channels = as_audio_channels(audio);
    if (channels < 1) channels = 2;
    if (channels > 8) channels = 8;

    std::vector<int> selected;
    std::string spec = opt_value(args, "--channels", "");
    if (spec == "all" || spec == "both") {
        for (int c = 0; c < channels; ++c) selected.push_back(c);
    } else if (!spec.empty()) {
        size_t pos = 0;
        while (pos <= spec.size()) {
            size_t comma = spec.find(',', pos);
            std::string part = spec.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
            char *end = nullptr;
            long c = std::strtol(part.c_str(), &end, 10);
            if (end != part.c_str() && c >= 0 && c < channels)
                selected.push_back((int)c);
            if (comma == std::string::npos) break;
            pos = comma + 1;
        }
    } else {
        int c = opt_int(args, "--channel", 0);
        if (c < 0 || c >= channels) c = 0;
        selected.push_back(c);
    }
    if (selected.empty()) selected.push_back(0);
    if (selected.size() > 8) selected.resize(8);

    std::vector<AsStats> st0(selected.size());
    std::vector<int> clip0(selected.size(), 0);
    std::vector<std::vector<double>> vpps(selected.size());
    std::vector<std::vector<double>> freqs(selected.size());
    int frames = 0;
    for (int r = 0; r < repeat; ++r) {
        std::vector<float> interleaved;
        std::vector<int> clips;
        frames = capture_interleaved(audio, seconds, &interleaved, &clips);
        for (size_t j = 0; j < selected.size(); ++j) {
            std::vector<float> frame;
            extract_channel(interleaved, channels, selected[j], &frame);
            AsStats st;
            as_stats_compute(frame.data(), (int)frame.size(), as_audio_rate(audio), &st);
            if (r == 0) {
                st0[j] = st;
                clip0[j] = (selected[j] < (int)clips.size()) ? clips[(size_t)selected[j]] : 0;
            }
            double peak_dbfs = as_linear_to_db(st.peak);
            double vrms = calibrated_vrms(have_cal ? &cal : nullptr, peak_dbfs);
            double vpp = vrms > 0.0 ? 2.0 * crest_factor(st) * vrms : 0.0;
            vpps[j].push_back(vpp);
            freqs[j].push_back(st.freq);
        }
    }

    AsReport rep;
    as_report_init(&rep, device.c_str(), as_input_mode_id(mode),
                   as_audio_rate(audio), frames, seconds);
    rep.confidence = have_cal ? cal.confidence : AS_CONF_ASSUMED;
    rep.fs_volts_rms = have_cal ? cal.fs_volts_rms : 0.0;
    std::snprintf(rep.calibration_id, sizeof(rep.calibration_id), "%s",
                  have_cal ? cal.device_id : "none");
    rep.channels = (int)selected.size();
    for (size_t j = 0; j < selected.size(); ++j) {
        const AsStats &st = st0[j];
        AsChannelMeasurement &m = rep.ch[j];
        m.channel = selected[j];
        std::snprintf(m.label, sizeof(m.label), "CH%d", selected[j] + 1);
        m.stats = st;
        m.peak_dbfs = as_linear_to_db(st.peak);
        m.rms_dbfs = as_linear_to_db(st.rms);
        m.vrms = calibrated_vrms(have_cal ? &cal : nullptr, m.peak_dbfs);
        m.vpp = m.vrms > 0.0 ? 2.0 * crest_factor(st) * m.vrms : 0.0;
        m.clip_count = clip0[j];
    }
    rep.repeat_count = repeat;
    rep.limits.max_vpp = opt_num(args, "--max-vpp", 0.0);
    rep.limits.min_rms = opt_num(args, "--min-rms", 0.0);
    rep.limits.max_clip_samples = opt_int(args, "--max-clip", 0);
    rep.limits.min_freq_hz = opt_num(args, "--min-freq", 20.0);
    rep.limits.max_freq_hz = opt_num(args, "--max-freq", 20000.0);
    as_report_evaluate(&rep);

    if (!vpps.empty() && vpps[0].size() > 1) {
        double mean = 0.0, fmean = 0.0;
        for (double x : vpps[0]) mean += x;
        for (double x : freqs[0]) fmean += x;
        mean /= (double)vpps[0].size();
        fmean /= (double)freqs[0].size();
        double var = 0.0, fvar = 0.0;
        for (size_t i = 0; i < vpps[0].size(); ++i) {
            var += (vpps[0][i] - mean) * (vpps[0][i] - mean);
            fvar += (freqs[0][i] - fmean) * (freqs[0][i] - fmean);
        }
        rep.vpp_std = std::sqrt(var / (double)(vpps[0].size() - 1));
        rep.freq_std = std::sqrt(fvar / (double)(freqs[0].size() - 1));
    }

    std::error_code ec;
    std::filesystem::create_directories(out_dir, ec);
    std::string json = out_dir + "/" + rep.run_id + ".json";
    std::string html = out_dir + "/" + rep.run_id + ".html";
    std::string pdf = out_dir + "/" + rep.run_id + ".pdf";
    int rc_json = as_report_write_json(&rep, json.c_str());
    int rc_html = as_report_write_html(&rep, html.c_str());
    int rc_pdf = as_report_write_pdf(&rep, pdf.c_str());

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
    for (int j = 0; j < rep.channels; ++j) {
        const AsChannelMeasurement &m = rep.ch[j];
        std::printf("  %s  peak %.1f dBFS  RMS %.1f dBFS  %.1f Hz  clip %d",
                    m.label, m.peak_dbfs, m.rms_dbfs, m.stats.freq, m.clip_count);
        if (m.vrms > 0.0) std::printf("  Vpp %.4f V  Vrms %.4f V", m.vpp, m.vrms);
        std::printf("\n");
    }
    if (repeat > 1)
        std::printf("  repeat %d · CH%d Vpp σ %.5f · freq σ %.2f Hz\n", repeat,
                    selected[0] + 1, rep.vpp_std, rep.freq_std);
    if (!have_cal) std::printf("  voltage unavailable: no calibration (dBFS only)\n");
    if (rc_json == AS_OK) std::printf("  JSON: %s\n", json.c_str());
    if (rc_html == AS_OK) std::printf("  HTML: %s\n", html.c_str());
    if (rc_pdf == AS_OK) std::printf("  PDF:  %s\n", pdf.c_str());

    as_audio_close(audio);
    as_safety_stop(guard);
    as_safety_destroy(guard);
    return rep.verdict == 2 ? 1 : 0;
}
int cmd_wow(const std::vector<std::string> &args) {
    double seconds = positional_num(args, 10.0);
    if (seconds < 3.0) seconds = 3.0;
    if (seconds > 60.0) seconds = 60.0;
    bool synthetic = has_flag(args, "--synthetic");
    double wow_pct = opt_num(args, "--wow", 0.3);
    double flutter_pct = opt_num(args, "--flutter", 0.1);
    double freq = opt_num(args, "--freq", 3000.0);
    double fs = kSampleRate;
    std::vector<float> x;
    if (synthetic) {
        int n = (int)(seconds * fs);
        x.resize((size_t)n);
        double phase = 0.0;
        for (int i = 0; i < n; ++i) {
            double t = (double)i / fs;
            double f = freq * (1.0 + wow_pct / 100.0 * std::sin(2.0 * 3.14159265358979323846 * 0.5 * t)
                                  + flutter_pct / 100.0 * std::sin(2.0 * 3.14159265358979323846 * 8.0 * t));
            phase += 2.0 * 3.14159265358979323846 * f / fs;
            x[(size_t)i] = (float)(0.5 * std::sin(phase));
        }
    } else {
        AsAudio *audio = as_audio_open(-1, -2, kSampleRate, 128);
        if (!audio) {
            std::fprintf(stderr, "audio open failed: %s\n", as_audio_last_error());
            return 1;
        }
        int channels = as_audio_channels(audio);
        if (channels < 1) channels = 1;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        std::vector<float> interleaved;
        std::vector<int> clips;
        capture_interleaved(audio, seconds, &interleaved, &clips);
        fs = as_audio_rate(audio);
        extract_channel(interleaved, channels, 0, &x);
        as_audio_close(audio);
    }
    AsWowResult r;
    if (as_wow_measure(x.data(), (int)x.size(), fs, &r) != 0) {
        std::fprintf(stderr, "wow/flutter measurement failed (need a clean 3 kHz tone)\n");
        return 1;
    }
    std::printf("%s %.2f s · %.1f Hz · %d periods\n",
                synthetic ? "synthetic" : "live", seconds, r.freq_hz, r.periods);
    std::printf("  wow %.3f %% RMS   flutter %.3f %% RMS   speed %.1f ppm\n",
                r.wow_pct, r.flutter_pct, r.speed_ppm);
    if (synthetic)
        std::printf("  injected: wow %.3f %% peak, flutter %.3f %% peak (RMS ≈ peak/√2)\n",
                    wow_pct, flutter_pct);
    else
        std::printf("  use a 3 kHz test tape / test tone; speed assumes 3000 Hz nominal.\n");
    return 0;
}

int cmd_crosstalk(const std::vector<std::string> &args) {
    double seconds = positional_num(args, 2.0);
    if (seconds < 0.5) seconds = 0.5;
    if (seconds > 30.0) seconds = 30.0;
    bool synthetic = has_flag(args, "--synthetic");
    double fs = kSampleRate;
    std::vector<float> a, b;
    if (synthetic) {
        int n = (int)(seconds * fs);
        a.resize((size_t)n);
        b.resize((size_t)n);
        for (int i = 0; i < n; ++i) {
            double t = (double)i / fs;
            double s = 0.1 * std::sin(2.0 * 3.14159265358979323846 * 1000.0 * t);
            a[(size_t)i] = (float)s;
            b[(size_t)i] = (float)(s * 0.001); /* -60 dB */
        }
    } else {
        AsAudio *audio = as_audio_open(-1, -2, kSampleRate, 128);
        if (!audio) {
            std::fprintf(stderr, "audio open failed: %s\n", as_audio_last_error());
            return 1;
        }
        int channels = as_audio_channels(audio);
        if (channels < 2) {
            std::fprintf(stderr, "crosstalk needs 2 channels; device reports %d\n", channels);
            as_audio_close(audio);
            return 1;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        std::vector<float> interleaved;
        std::vector<int> clips;
        capture_interleaved(audio, seconds, &interleaved, &clips);
        fs = as_audio_rate(audio);
        extract_channel(interleaved, channels, 0, &a);
        extract_channel(interleaved, channels, 1, &b);
        as_audio_close(audio);
        std::printf("live: drive CH1 only; CH2 should be silent except leakage.\n");
    }
    double db = 0.0;
    if (as_crosstalk_db(a.data(), b.data(), (int)std::min(a.size(), b.size()), &db) != 0) {
        std::fprintf(stderr, "crosstalk measurement failed (CH1 is silent)\n");
        return 1;
    }
    std::printf("crosstalk CH1 -> CH2: %.2f dB", db);
    if (synthetic) std::printf(" (injected -60 dB)");
    std::printf("\n");
    return 0;
}

int cmd_impedance(const std::vector<std::string> &args) {
    double r = opt_num(args, "--r", 1000.0);
    double vsrc = opt_num(args, "--vsrc", 1.0);
    double vdut = opt_num(args, "--vdut", -1.0);
    double zsim = opt_num(args, "--simulate", -1.0);
    if (zsim > 0.0) {
        if (!(r > 0.0) || !(vsrc > 0.0)) {
            std::fprintf(stderr, "need positive --r and --vsrc\n");
            return 1;
        }
        vdut = vsrc * zsim / (r + zsim);
    }
    if (vdut < 0.0) {
        std::fprintf(stderr, "usage: impedance --r R --vsrc V --vdut V | --simulate Z\n");
        return 1;
    }
    double z = 0.0;
    if (as_impedance(r, vdut, vsrc, &z) != 0) {
        std::fprintf(stderr, "impedance invalid: need 0 <= Vdut < Vsource\n");
        return 1;
    }
    std::printf("series R %.1f ohm · Vsource %.4f V · Vdut %.4f V\n", r, vsrc, vdut);
    std::printf("  estimated Z %.2f ohm", z);
    if (zsim > 0.0) std::printf(" (injected %.2f ohm)", zsim);
    std::printf("\n");
    return 0;
}

int cmd_azimuth(const std::vector<std::string> &args) {
    double seconds = positional_num(args, 2.0);
    if (seconds < 0.5) seconds = 0.5;
    if (seconds > 30.0) seconds = 30.0;
    bool synthetic = has_flag(args, "--synthetic");
    double phase_in = opt_num(args, "--phase", 10.0);
    double fs = kSampleRate;
    std::vector<float> l, r;
    if (synthetic) {
        int n = (int)(seconds * fs);
        l.resize((size_t)n);
        r.resize((size_t)n);
        double pr = phase_in * 3.14159265358979323846 / 180.0;
        for (int i = 0; i < n; ++i) {
            double t = (double)i / fs;
            l[(size_t)i] = (float)(0.5 * std::sin(2.0 * 3.14159265358979323846 * 10000.0 * t));
            r[(size_t)i] = (float)(0.5 * std::sin(2.0 * 3.14159265358979323846 * 10000.0 * t + pr));
        }
    } else {
        AsAudio *audio = as_audio_open(-1, -2, kSampleRate, 128);
        if (!audio) {
            std::fprintf(stderr, "audio open failed: %s\n", as_audio_last_error());
            return 1;
        }
        int channels = as_audio_channels(audio);
        if (channels < 2) {
            std::fprintf(stderr, "azimuth needs 2 channels; device reports %d\n", channels);
            as_audio_close(audio);
            return 1;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        std::vector<float> interleaved;
        std::vector<int> clips;
        capture_interleaved(audio, seconds, &interleaved, &clips);
        fs = as_audio_rate(audio);
        extract_channel(interleaved, channels, 0, &l);
        extract_channel(interleaved, channels, 1, &r);
        as_audio_close(audio);
    }
    AsPhaseResult pr;
    if (as_phase_measure(l.data(), r.data(), (int)std::min(l.size(), r.size()), fs, &pr) != 0) {
        std::fprintf(stderr, "azimuth measurement failed (need a 10 kHz test tone)\n");
        return 1;
    }
    std::printf("azimuth L/R phase: %.2f deg at %.1f Hz\n", pr.phase_deg, pr.freq_hz);
    std::printf("  adjust the head until phase is near 0 and L+R sum is maximum.\n");
    if (synthetic) std::printf("  injected phase: %.2f deg\n", phase_in);
    return 0;
}

int cmd_compare(const std::vector<std::string> &args) {
    if (args.size() < 4) {
        std::fprintf(stderr, "usage: compare <before.json> <after.json>\n");
        return 1;
    }
    auto read_num = [](const std::string &path, const char *key, double *out) {
        std::ifstream in(path);
        if (!in) return false;
        std::stringstream ss;
        ss << in.rdbuf();
        std::string text = ss.str();
        std::string pat = std::string("\"") + key + "\"";
        size_t p = text.find(pat);
        if (p == std::string::npos) return false;
        p = text.find(':', p + pat.size());
        if (p == std::string::npos) return false;
        char *end = nullptr;
        double v = std::strtod(text.c_str() + p + 1, &end);
        if (end == text.c_str() + p + 1) return false;
        *out = v;
        return true;
    };
    const char *keys[] = {"peak_dbfs", "rms_dbfs", "vpp", "freq_hz", "clip_count"};
    double before[5] = {0}, after[5] = {0};
    for (int i = 0; i < 5; ++i) {
        if (!read_num(args[2], keys[i], &before[i]) ||
            !read_num(args[3], keys[i], &after[i])) {
            std::fprintf(stderr, "could not read %s from both reports\n", keys[i]);
            return 1;
        }
    }
    std::printf("compare %s -> %s\n", args[2].c_str(), args[3].c_str());
    std::printf("  peak dBFS: %.2f -> %.2f (%+.2f)\n", before[0], after[0], after[0] - before[0]);
    std::printf("  RMS dBFS : %.2f -> %.2f (%+.2f)\n", before[1], after[1], after[1] - before[1]);
    std::printf("  Vpp      : %.4f -> %.4f (%+.4f)\n", before[2], after[2], after[2] - before[2]);
    std::printf("  freq Hz  : %.1f -> %.1f (%+.1f)\n", before[3], after[3], after[3] - before[3]);
    std::printf("  clip     : %.0f -> %.0f\n", before[4], after[4]);
    return 0;
}


void make_tone_signal(std::vector<float> *x, int n, double fs, double freq,
                      double level_dbfs, double thd_pct) {
    x->assign((size_t)n, 0.0f);
    double amp = std::pow(10.0, level_dbfs / 20.0);
    double h2 = amp * thd_pct / 100.0 * 0.8;
    double h3 = amp * thd_pct / 100.0 * 0.6;
    for (int i = 0; i < n; ++i) {
        double t = (double)i / fs;
        double v = amp * std::sin(2.0 * 3.14159265358979323846 * freq * t)
                 + h2 * std::sin(2.0 * 3.14159265358979323846 * 2.0 * freq * t)
                 + h3 * std::sin(2.0 * 3.14159265358979323846 * 3.0 * freq * t);
        (*x)[(size_t)i] = (float)v;
    }
}

int cmd_thd(const std::vector<std::string> &args) {
    double seconds = positional_num(args, 2.0);
    if (seconds < 0.5) seconds = 0.5;
    if (seconds > 30.0) seconds = 30.0;
    bool synthetic = has_flag(args, "--synthetic");
    double freq = opt_num(args, "--freq", 1000.0);
    double level = opt_num(args, "--level", -6.0);
    double thd_in = opt_num(args, "--thd", 0.1);
    double fs = kSampleRate;
    std::vector<float> x;
    if (synthetic) {
        make_tone_signal(&x, (int)(seconds * fs), fs, freq, level, thd_in);
    } else {
        AsAudio *audio = as_audio_open(-1, -2, kSampleRate, 128);
        if (!audio) {
            std::fprintf(stderr, "audio open failed: %s\n", as_audio_last_error());
            return 1;
        }
        int channels = as_audio_channels(audio);
        if (channels < 1) channels = 1;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        std::vector<float> interleaved;
        std::vector<int> clips;
        capture_interleaved(audio, seconds, &interleaved, &clips);
        fs = as_audio_rate(audio);
        extract_channel(interleaved, channels, 0, &x);
        as_audio_close(audio);
    }
    AsToneResult r;
    if (as_tone_analyze(x.data(), (int)x.size(), fs, &r) != 0) {
        std::fprintf(stderr, "THD analysis failed (no stable tone)\n");
        return 1;
    }
    std::printf("%s %.2f s · %.1f Hz\n", synthetic ? "synthetic" : "live", seconds, r.freq_hz);
    std::printf("  THD %.4f %% (%.2f dBc)   THD+N %.4f %% (%.2f dB)\n",
                r.thd_pct, r.thd_db, r.thdn_pct, r.thdn_db);
    std::printf("  SNR %.2f dB   SINAD %.2f dB   SFDR %.2f dB   ENOB %.2f bit\n",
                r.snr_db, r.sinad_db, r.sfdr_db, r.enob);
    if (synthetic) std::printf("  injected THD≈%.3f %%\n", thd_in);
    return 0;
}

int cmd_imd(const std::vector<std::string> &args) {
    double seconds = positional_num(args, 2.0);
    if (seconds < 0.5) seconds = 0.5;
    if (seconds > 30.0) seconds = 30.0;
    bool synthetic = has_flag(args, "--synthetic");
    std::string std_name = opt_value(args, "--standard", "smpte");
    int din = (std_name == "din") ? 1 : 0;
    double f1 = din ? 250.0 : 60.0;
    double f2 = din ? 8000.0 : 7000.0;
    double fs = kSampleRate;
    std::vector<float> x;
    if (synthetic) {
        int n = (int)(seconds * fs);
        x.assign((size_t)n, 0.0f);
        double a1 = 0.4, a2 = 0.1;
        double side = a2 * 0.0005; /* four sidebands ≈ 0.1% total */
        for (int i = 0; i < n; ++i) {
            double t = (double)i / fs;
            double v = a1 * std::sin(2.0 * 3.14159265358979323846 * f1 * t)
                     + a2 * std::sin(2.0 * 3.14159265358979323846 * f2 * t)
                     + side * std::sin(2.0 * 3.14159265358979323846 * (f2 - f1) * t)
                     + side * std::sin(2.0 * 3.14159265358979323846 * (f2 + f1) * t)
                     + side * std::sin(2.0 * 3.14159265358979323846 * (f2 - 2.0 * f1) * t)
                     + side * std::sin(2.0 * 3.14159265358979323846 * (f2 + 2.0 * f1) * t);
            x[(size_t)i] = (float)v;
        }
    } else {
        AsAudio *audio = as_audio_open(-1, -2, kSampleRate, 128);
        if (!audio) {
            std::fprintf(stderr, "audio open failed: %s\n", as_audio_last_error());
            return 1;
        }
        int channels = as_audio_channels(audio);
        if (channels < 1) channels = 1;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        std::vector<float> interleaved;
        std::vector<int> clips;
        capture_interleaved(audio, seconds, &interleaved, &clips);
        fs = as_audio_rate(audio);
        extract_channel(interleaved, channels, 0, &x);
        as_audio_close(audio);
    }
    double imd = 0.0, ff1 = 0.0, ff2 = 0.0;
    if (as_imd_analyze(x.data(), (int)x.size(), fs, din, &imd, &ff1, &ff2) != 0) {
        std::fprintf(stderr, "IMD analysis failed (need the two test tones)\n");
        return 1;
    }
    std::printf("%s %.2f s · %s · %.1f Hz + %.1f Hz\n",
                synthetic ? "synthetic" : "live", seconds,
                din ? "DIN" : "SMPTE", ff1, ff2);
    std::printf("  IMD %.4f %% (%.2f dBc)\n", imd, 20.0 * std::log10(std::max(imd, 1e-9) / 100.0));
    if (synthetic) std::printf("  injected IMD≈0.100 %%\n");
    return 0;
}

int cmd_sweep(const std::vector<std::string> &args) {
    double start = opt_num(args, "--start", -40.0);
    double end = opt_num(args, "--end", 0.0);
    double step = opt_num(args, "--step", 6.0);
    double freq = opt_num(args, "--freq", 1000.0);
    double thd_in = opt_num(args, "--thd", 0.1);
    if (step <= 0.0 || end < start) {
        std::fprintf(stderr, "invalid sweep range\n");
        return 1;
    }
    std::string csv = opt_value(args, "--csv", "");
    FILE *f = csv.empty() ? nullptr : std::fopen(csv.c_str(), "wb");
    if (!csv.empty() && !f) {
        std::fprintf(stderr, "cannot write %s\n", csv.c_str());
        return 1;
    }
    if (f) std::fprintf(f, "level_dbfs,thd_pct,thdn_pct,snr_db,sinad_db\n");
    std::printf("%10s %10s %10s %10s\n", "dBFS", "THD%", "THD+N%", "SNR dB");
    for (double level = start; level <= end + 1e-9; level += step) {
        std::vector<float> x;
        make_tone_signal(&x, kFftSize, kSampleRate, freq, level, thd_in);
        AsToneResult r;
        if (as_tone_analyze(x.data(), (int)x.size(), kSampleRate, &r) != 0) continue;
        std::printf("%10.1f %10.4f %10.4f %10.2f\n", level, r.thd_pct, r.thdn_pct, r.snr_db);
        if (f) std::fprintf(f, "%.1f,%.6f,%.6f,%.6f,%.6f\n",
                            level, r.thd_pct, r.thdn_pct, r.snr_db, r.sinad_db);
    }
    if (f) std::fclose(f);
    return 0;
}


int cmd_freqsweep(const std::vector<std::string> &args) {
    double start = opt_num(args, "--start", 20.0);
    double end = opt_num(args, "--end", 20000.0);
    int points = opt_int(args, "--points", 10);
    double level = opt_num(args, "--level", -6.0);
    double thd_in = opt_num(args, "--thd", 0.1);
    if (start < 10.0 || end <= start || points < 2 || points > 100) {
        std::fprintf(stderr, "invalid frequency sweep range\n");
        return 1;
    }
    std::string csv = opt_value(args, "--csv", "");
    FILE *f = csv.empty() ? nullptr : std::fopen(csv.c_str(), "wb");
    if (!csv.empty() && !f) {
        std::fprintf(stderr, "cannot write %s\n", csv.c_str());
        return 1;
    }
    if (f) std::fprintf(f, "freq_hz,thd_pct,thdn_pct,snr_db\n");
    std::printf("%10s %10s %10s %10s\n", "Hz", "THD%", "THD+N%", "SNR dB");
    const int n = 32768;
    for (int i = 0; i < points; ++i) {
        double t = points > 1 ? (double)i / (double)(points - 1) : 0.0;
        double freq = start * std::pow(end / start, t);
        std::vector<float> x;
        make_tone_signal(&x, n, kSampleRate, freq, level, thd_in);
        AsToneResult r;
        if (as_tone_analyze(x.data(), n, kSampleRate, &r) != 0) continue;
        std::printf("%10.1f %10.4f %10.4f %10.2f\n", freq, r.thd_pct, r.thdn_pct, r.snr_db);
        if (f) std::fprintf(f, "%.1f,%.6f,%.6f,%.6f\n",
                            freq, r.thd_pct, r.thdn_pct, r.snr_db);
    }
    if (f) std::fclose(f);
    return 0;
}


struct PlanStep {
    std::string id;
    std::string label;
    std::string unit;
    double value;
    bool pass;
    std::string detail;
};

struct PlanDef {
    const char *id;
    const char *label;
    const char *description;
    const char *steps[5];
    int count;
};

static const PlanDef kPlans[] = {
    {"quick", "Quick check", "THD + IMD smoke test", {"thd", "imd", nullptr, nullptr, nullptr}, 2},
    {"pedal", "Pedal / stompbox", "THD + IMD", {"thd", "imd", nullptr, nullptr, nullptr}, 2},
    {"tape", "Tape deck", "Wow/flutter + azimuth + THD", {"wow", "azimuth", "thd", nullptr, nullptr}, 3},
    {"mixer", "Mixer channel", "THD + IMD + crosstalk", {"thd", "imd", "crosstalk", nullptr, nullptr}, 3},
    {"headphone", "Headphone amp", "THD + IMD + crosstalk", {"thd", "imd", "crosstalk", nullptr, nullptr}, 3},
};

static const PlanDef *find_plan(const std::string &id) {
    for (const PlanDef &p : kPlans) if (id == p.id) return &p;
    return nullptr;
}

static bool synth_thd(double *out) {
    std::vector<float> x;
    make_tone_signal(&x, 32768, kSampleRate, 1000.0, -6.0, 0.1);
    AsToneResult r;
    if (as_tone_analyze(x.data(), (int)x.size(), kSampleRate, &r) != 0) return false;
    *out = r.thd_pct;
    return true;
}

static bool synth_imd(double *out) {
    int n = 32768;
    std::vector<float> x((size_t)n);
    for (int i = 0; i < n; ++i) {
        double t = (double)i / kSampleRate;
        double f1 = 60.0, f2 = 7000.0, side = 0.1 * 0.0005;
        double v = 0.4 * std::sin(2.0 * 3.14159265358979323846 * f1 * t)
                 + 0.1 * std::sin(2.0 * 3.14159265358979323846 * f2 * t)
                 + side * std::sin(2.0 * 3.14159265358979323846 * (f2 - f1) * t)
                 + side * std::sin(2.0 * 3.14159265358979323846 * (f2 + f1) * t)
                 + side * std::sin(2.0 * 3.14159265358979323846 * (f2 - 2.0 * f1) * t)
                 + side * std::sin(2.0 * 3.14159265358979323846 * (f2 + 2.0 * f1) * t);
        x[(size_t)i] = (float)v;
    }
    double imd = 0.0;
    if (as_imd_analyze(x.data(), n, kSampleRate, 0, &imd, nullptr, nullptr) != 0) return false;
    *out = imd;
    return true;
}

static bool synth_wow(double *wow, double *flutter) {
    int n = 5 * (int)kSampleRate;
    std::vector<float> x((size_t)n);
    double phase = 0.0;
    for (int i = 0; i < n; ++i) {
        double t = (double)i / kSampleRate;
        double f = 3000.0 * (1.0 + 0.003 * std::sin(2.0 * 3.14159265358979323846 * 0.5 * t)
                                + 0.001 * std::sin(2.0 * 3.14159265358979323846 * 8.0 * t));
        phase += 2.0 * 3.14159265358979323846 * f / kSampleRate;
        x[(size_t)i] = (float)(0.5 * std::sin(phase));
    }
    AsWowResult r;
    if (as_wow_measure(x.data(), n, kSampleRate, &r) != 0) return false;
    *wow = r.wow_pct;
    *flutter = r.flutter_pct;
    return true;
}

static bool synth_azimuth(double *phase) {
    int n = 2 * (int)kSampleRate;
    std::vector<float> l((size_t)n), r((size_t)n);
    double pr = 10.0 * 3.14159265358979323846 / 180.0;
    for (int i = 0; i < n; ++i) {
        double t = (double)i / kSampleRate;
        l[(size_t)i] = (float)(0.5 * std::sin(2.0 * 3.14159265358979323846 * 10000.0 * t));
        r[(size_t)i] = (float)(0.5 * std::sin(2.0 * 3.14159265358979323846 * 10000.0 * t + pr));
    }
    AsPhaseResult res;
    if (as_phase_measure(l.data(), r.data(), n, kSampleRate, &res) != 0) return false;
    *phase = res.phase_deg;
    return true;
}

static bool synth_crosstalk(double *db) {
    int n = 2 * (int)kSampleRate;
    std::vector<float> a((size_t)n), b((size_t)n);
    for (int i = 0; i < n; ++i) {
        double t = (double)i / kSampleRate;
        double s = 0.1 * std::sin(2.0 * 3.14159265358979323846 * 1000.0 * t);
        a[(size_t)i] = (float)s;
        b[(size_t)i] = (float)(s * 0.001);
    }
    return as_crosstalk_db(a.data(), b.data(), n, db) == 0;
}

int cmd_templates() {
    std::printf("%-12s %-24s %s\n", "id", "label", "steps");
    for (const PlanDef &p : kPlans) {
        std::printf("%-12s %-24s ", p.id, p.label);
        for (int i = 0; i < p.count; ++i)
            std::printf("%s%s", i ? "," : "", p.steps[i]);
        std::printf("\n");
    }
    return 0;
}

int cmd_repair(const std::vector<std::string> &args) {
    std::string id = opt_value(args, "--template", "quick");
    const PlanDef *plan = find_plan(id);
    if (!plan) {
        std::fprintf(stderr, "unknown template '%s'; use templates\n", id.c_str());
        return 1;
    }
    if (has_flag(args, "--live")) {
        std::fprintf(stderr, "live repair plans are not wired yet; use --synthetic for now.\n");
        return 1;
    }
    std::string out_dir = opt_value(args, "--out", "repairs");
    std::error_code ec;
    std::filesystem::create_directories(out_dir, ec);
    std::vector<PlanStep> results;
    for (int i = 0; i < plan->count; ++i) {
        std::string step = plan->steps[i];
        if (step == "thd") {
            double v = 0.0;
            bool ok = synth_thd(&v);
            results.push_back({"thd", "THD", "%", v, ok && v <= 0.5, ok ? "" : "analysis failed"});
        } else if (step == "imd") {
            double v = 0.0;
            bool ok = synth_imd(&v);
            results.push_back({"imd", "IMD SMPTE", "%", v, ok && v <= 0.5, ok ? "" : "analysis failed"});
        } else if (step == "wow") {
            double w = 0.0, f = 0.0;
            bool ok = synth_wow(&w, &f);
            results.push_back({"wow", "Wow", "% RMS", w, ok && w <= 0.5, ok ? "" : "analysis failed"});
            results.push_back({"flutter", "Flutter", "% RMS", f, ok && f <= 0.2, ok ? "" : "analysis failed"});
        } else if (step == "azimuth") {
            double v = 0.0;
            bool ok = synth_azimuth(&v);
            results.push_back({"azimuth", "Azimuth phase", "deg", v, ok && std::fabs(v) <= 15.0, ok ? "" : "analysis failed"});
        } else if (step == "crosstalk") {
            double v = 0.0;
            bool ok = synth_crosstalk(&v);
            results.push_back({"crosstalk", "Crosstalk", "dB", v, ok && v <= -40.0, ok ? "" : "analysis failed"});
        }
    }
    bool pass = true;
    for (const PlanStep &r : results) if (!r.pass) pass = false;
    std::string base = out_dir + "/repair-" + id;
    std::string json = base + ".json";
    std::string pdf = base + ".pdf";
    {
        std::ofstream out(json);
        out << "{\n  \"template\": \"" << id << "\",\n  \"verdict\": \""
            << (pass ? "PASS" : "FAIL") << "\",\n  \"steps\": [\n";
        for (size_t i = 0; i < results.size(); ++i) {
            const PlanStep &r = results[i];
            out << "    {\"id\":\"" << r.id << "\",\"label\":\"" << r.label
                << "\",\"value\":" << r.value << ",\"unit\":\"" << r.unit
                << "\",\"pass\":" << (r.pass ? "true" : "false") << "}";
            out << (i + 1 == results.size() ? "\n" : ",\n");
        }
        out << "  ]\n}\n";
    }
    AsPdf *doc = as_pdf_create("AudioScope repair plan");
    if (doc) {
        char line[256];
        as_pdf_text(doc, 50, 790, 18, AS_BRAND);
        as_pdf_text(doc, 50, 768, 10, "Repair plan summary");
        as_pdf_line(doc, 50, 758, 545, 758);
        snprintf(line, sizeof(line), "Template: %s (%s)", plan->id, plan->label);
        as_pdf_text(doc, 50, 742, 10, line);
        snprintf(line, sizeof(line), "Verdict: %s", pass ? "PASS" : "FAIL");
        as_pdf_text(doc, 50, 724, 14, line);
        as_pdf_line(doc, 50, 714, 545, 714);
        double y = 694;
        for (const PlanStep &r : results) {
            snprintf(line, sizeof(line), "%-18s %10.4f %-6s  %s",
                     r.label.c_str(), r.value, r.unit.c_str(), r.pass ? "PASS" : "FAIL");
            as_pdf_text(doc, 50, y, 10, line);
            y -= 18;
        }
        as_pdf_line(doc, 50, y + 6, 545, y + 6);
        as_pdf_text(doc, 50, y - 8, 8,
                    "Synthetic verification only; live DUT loopback is not wired in 0.6-alpha.");
        as_pdf_text(doc, 50, 60, 8, "AudioScope by BerryStudio 0.7-alpha");
        as_pdf_finish(doc, pdf.c_str());
        as_pdf_destroy(doc);
    }
    std::printf("repair plan: %s · %s\n", plan->id, pass ? "PASS" : "FAIL");
    for (const PlanStep &r : results)
        std::printf("  %-18s %10.4f %-6s %s\n", r.label.c_str(), r.value,
                    r.unit.c_str(), r.pass ? "PASS" : "FAIL");
    std::printf("  JSON: %s\n  PDF:  %s\n", json.c_str(), pdf.c_str());
    return pass ? 0 : 1;
}



int cmd_ui(const std::vector<std::string> &args) {
    std::string web_src = opt_value(args, "--web-src", "web");
    std::string out_dir = opt_value(args, "--out", "build/ui");
    std::error_code ec;
    if (!std::filesystem::exists(web_src, ec)) {
        if (std::filesystem::exists("alpha/web", ec)) web_src = "alpha/web";
        else {
            std::fprintf(stderr, "web UI source not found: %s (run from alpha/ or pass --web-src)\n", web_src.c_str());
            return 1;
        }
    }
    std::filesystem::create_directories(out_dir, ec);
    for (const auto &entry : std::filesystem::directory_iterator(web_src, ec)) {
        std::string name = entry.path().filename().string();
        if (name == "docs" || name == "scripts") continue;
        std::filesystem::copy(entry.path(), std::filesystem::path(out_dir) / entry.path().filename(),
                              std::filesystem::copy_options::recursive |
                              std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) break;
    }
    if (ec) {
        std::fprintf(stderr, "cannot copy web UI: %s\n", ec.message().c_str());
        return 1;
    }
    if (has_flag(args, "--live"))
        std::fprintf(stderr, "live UI snapshot is not wired yet; using synthetic data.\n");

    const double fs = kSampleRate;
    const int n = 8192;
    const int scope_n = 720;
    std::vector<float> scope_wave((size_t)scope_n);
    make_tone_signal(&scope_wave, scope_n, fs, 1000.0, -6.0, 0.1);
    std::vector<float> wave((size_t)n);
    make_tone_signal(&wave, n, fs, 1000.0, -6.0, 0.1);
    AsStats st;
    as_stats_compute(scope_wave.data(), scope_n, fs, &st);

    std::vector<double> x((size_t)n), win((size_t)n), mag((size_t)n / 2 + 1);
    for (int i = 0; i < n; ++i) x[(size_t)i] = wave[(size_t)i];
    as_window_hann(win.data(), n);
    for (int i = 0; i < n; ++i) x[(size_t)i] *= win[(size_t)i];
    AsFft fft;
    if (as_fft_init(&fft, n) != 0) {
        std::fprintf(stderr, "fft init failed\n");
        return 1;
    }
    as_fft_forward(&fft, x.data(), mag.data(), n);
    double maxmag = 1e-12;
    for (int i = 1; i < n / 2; ++i) maxmag = std::max(maxmag, mag[(size_t)i]);
    AsToneResult thd;
    as_tone_analyze(wave.data(), n, fs, &thd);

    std::ostringstream js;
    js << "window.AUDIOSCOPE_DATA = {\n";
    std::time_t now = std::time(nullptr);
    char stamp[32];
    std::strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M", std::localtime(&now));
    js << "  meta: {version:\"0.7.0-alpha.1\", platform:\"macOS · CoreAudio\", mode:\"Inst\","
       << " device:\"Studio 1824c\", rate:48000, channels:2, source:\"synthetic snapshot\","
       << " timestamp:\"" << stamp << "\"},\n";
    js << "  scope: {waveform:[";
    for (int i = 0; i < scope_n; ++i) {
        if (i) js << ",";
        js << std::fixed << std::setprecision(5) << scope_wave[(size_t)i];
    }
    js << "], peak_dbfs:" << std::setprecision(2) << as_linear_to_db(st.peak)
       << ", rms_dbfs:" << as_linear_to_db(st.rms)
       << ", freq_hz:" << std::setprecision(1) << st.freq
       << ", crest:" << std::setprecision(2) << 20.0 * std::log10(std::max(crest_factor(st), 1e-6))
       << ", vpp:" << std::setprecision(4) << 2.0 * st.peak
       << ", clip:0, channel:0},\n";
    js << "  spectrum: {freqs:[";
    const int bins = 220;
    for (int i = 0; i < bins; ++i) {
        double f = 20.0 * std::pow(1000.0, (double)i / (double)(bins - 1));
        if (i) js << ",";
        js << std::setprecision(1) << f;
    }
    js << "], db:[";
    for (int i = 0; i < bins; ++i) {
        double f = 20.0 * std::pow(1000.0, (double)i / (double)(bins - 1));
        int bin = (int)std::lround(f * n / fs);
        if (bin < 1) bin = 1;
        if (bin > n / 2 - 1) bin = n / 2 - 1;
        double db = 20.0 * std::log10(std::max(mag[(size_t)bin] / maxmag, 1e-6));
        if (db < -100.0) db = -100.0;
        if (i) js << ",";
        js << std::setprecision(1) << db;
    }
    js << "], thd:" << std::setprecision(4) << thd.thd_pct
       << ", thdn:" << thd.thdn_pct
       << ", snr:" << std::setprecision(2) << thd.snr_db
       << ", sinad:" << thd.sinad_db
       << ", sfdr:" << thd.sfdr_db
       << ", enob:" << std::setprecision(2) << thd.enob << "},\n";

    js << "  xy: {points:[";
    for (int i = 0; i < 1600; ++i) {
        double t = (double)i / fs;
        double g = std::pow(10.0, 3.0 / 20.0);
        double a = 0.5 * std::sin(2.0 * 3.14159265358979323846 * 1000.0 * t);
        double b = 0.5 * g * std::sin(2.0 * 3.14159265358979323846 * 1000.0 * t + 45.0 * 3.14159265358979323846 / 180.0);
        if (i) js << ",";
        js << "[" << std::setprecision(5) << a << "," << b << "]";
    }
    js << "], phase_deg:45.0, gain_db:3.0, freq_hz:1000.0, drift_ppm:0},\n";

    js << "  channels:[\n";
    for (int c = 0; c < 8; ++c) {
        double peak = (c < 2) ? as_linear_to_db(st.peak) - (c ? 0.2 : 0.0) : -96.0 + c * 0.1;
        double rms = (c < 2) ? as_linear_to_db(st.rms) - (c ? 0.2 : 0.0) : -102.0 + c * 0.1;
        js << "    {label:\"CH" << (c + 1) << "\", peak_dbfs:" << std::setprecision(1) << peak
           << ", rms_dbfs:" << rms << ", freq_hz:" << (c < 2 ? 1000 : 0)
           << ", vpp:" << std::setprecision(4) << (c < 2 ? 1.0 - c * 0.02 : 0.0)
           << ", clip:0, verdict:\"" << (c < 2 ? "PASS" : "—") << "\"}"
           << (c < 7 ? "," : "") << "\n";
    }
    js << "  ],\n";

    double wow = 0.0, flutter = 0.0, azimuth = 0.0, plan_thd = 0.0;
    synth_wow(&wow, &flutter);
    synth_azimuth(&azimuth);
    synth_thd(&plan_thd);
    js << "  repair:{template:\"tape\", label:\"Tape deck\", verdict:\"PASS\", steps:[\n";
    js << "    {label:\"Wow\", value:" << std::setprecision(4) << wow << ", unit:\"% RMS\", pass:true},\n";
    js << "    {label:\"Flutter\", value:" << flutter << ", unit:\"% RMS\", pass:true},\n";
    js << "    {label:\"Azimuth\", value:" << std::setprecision(2) << azimuth << ", unit:\"deg\", pass:true},\n";
    js << "    {label:\"THD\", value:" << std::setprecision(4) << plan_thd << ", unit:\"%\", pass:true}\n";
    js << "  ]},\n";

    js << "  reports:[\n"
       << "    {name:\"repair-tape.pdf\", kind:\"维修计划\", verdict:\"PASS\", time:\"13:57\"},\n"
       << "    {name:\"run-20261107-043914.pdf\", kind:\"测量报告\", verdict:\"WARN\", time:\"04:39\"},\n"
       << "    {name:\"run-20261107-043823.pdf\", kind:\"测量报告\", verdict:\"WARN\", time:\"04:38\"}\n"
       << "  ],\n";
    js << "  calibration:{id:\"none\", confidence:\"ASSUMED\", fs_vrms:0.0,"
       << " note:\"未校准：只显示 dBFS；电压需要 measured / datasheet / assumed。\"}\n";
    js << "};\n";

    std::string index_path = out_dir + "/index.html";
    std::ifstream in(index_path);
    if (!in) {
        std::fprintf(stderr, "cannot read %s\n", index_path.c_str());
        return 1;
    }
    std::string html((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::string marker = "<!--DATA-->";
    size_t pos = html.find(marker);
    if (pos == std::string::npos) {
        std::fprintf(stderr, "UI template missing %s\n", marker.c_str());
        return 1;
    }
    html.replace(pos, marker.size(), "<script>" + js.str() + "</script>");
    std::ofstream out(index_path, std::ios::binary);
    out << html;
    out.close();
    as_fft_free(&fft);
    std::printf("UI: %s/index.html\n", out_dir.c_str());
    std::printf("serve: python3 -m http.server --directory %s 8124\n", out_dir.c_str());
    return 0;
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
    if (cmd == "xy") return cmd_xy(args);
    if (cmd == "drift") return cmd_drift(args);
    if (cmd == "spectrogram") return cmd_spectrogram(args);
    if (cmd == "wow") return cmd_wow(args);
    if (cmd == "crosstalk") return cmd_crosstalk(args);
    if (cmd == "impedance") return cmd_impedance(args);
    if (cmd == "azimuth") return cmd_azimuth(args);
    if (cmd == "compare") return cmd_compare(args);
    if (cmd == "thd") return cmd_thd(args);
    if (cmd == "imd") return cmd_imd(args);
    if (cmd == "sweep") return cmd_sweep(args);
    if (cmd == "freqsweep") return cmd_freqsweep(args);
    if (cmd == "templates") return cmd_templates();
    if (cmd == "repair") return cmd_repair(args);
    if (cmd == "ui") return cmd_ui(args);
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
