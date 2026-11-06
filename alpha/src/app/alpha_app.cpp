#include "app/alpha_app.hpp"

#include "core/api.h"
#include "core/fft.h"
#include "core/generator.h"
#include "core/stats.h"
#include "core/window.h"
#include "platform/audio_backend.h"
#include "ui/ascii_canvas.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace audioscope {
namespace {

constexpr int kSampleRate = 48000;
constexpr int kFrames = 480;

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
        "%s %s — sound-card audio repair workstation (alpha)\n"
        "\n"
        "Usage:\n"
        "  audioscope-alpha version\n"
        "  audioscope-alpha devices\n"
        "  audioscope-alpha monitor [seconds]      live ASCII scope from input\n"
        "  audioscope-alpha tone [hz] [amp] [sec] output sine test tone\n"
        "  audioscope-alpha scope                  synthetic 1 kHz scope demo\n"
        "  audioscope-alpha fft                    synthetic 1 kHz FFT demo\n"
        "  audioscope-alpha units                  unit conversion demo\n"
        "  audioscope-alpha safety                 SafetyGuard state demo\n"
        "  audioscope-alpha session <dir>          write a session.jsonl demo\n"
        "  audioscope-alpha calibrate <path> [fs_v] [device]\n"
        "  audioscope-alpha selftest               run core self tests\n"
        "  audioscope-alpha help\n"
        "\n"
        "Alpha caveat: the software can warn and mute, but it cannot protect\n"
        "the sound card from over-voltage. Use a protected front end.\n",
        as_brand_string(), as_version_string());
}

std::string render_scope(const std::vector<float> &mono, double fs,
                         AsStats *stats) {
    const int w = 78;
    const int h = 22;
    AsciiCanvas c(w, h);
    c.clear(' ');
    c.frame();
    c.text(2, 1, "AudioScope by BerryStudio  |  SCOPE  |  1 kHz demo");

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
    char line[96];
    std::snprintf(line, sizeof(line), "Vpp %.3f V   RMS %.3f V   %6.1f Hz",
                  st.vpp, st.rms, st.freq);
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
    char line[96];
    double f = as_fft_bin_hz(peak_bin, 4096, fs);
    std::snprintf(line, sizeof(line), "peak %.1f Hz   bin %d   log axis",
                  f, peak_bin);
    c.text(2, h - 2, line);
    return c.to_string();
}

int run_self_test() {
    int failures = 0;
    auto check = [&](bool ok, const char *name) {
        std::printf("  %s  %s\n", ok ? "ok  " : "FAIL", name);
        if (!ok) ++failures;
    };

    check(std::strcmp(as_version_string(), AS_ALPHA_VERSION) == 0, "version string");
    check(std::strcmp(as_brand_string(), AS_BRAND) == 0, "brand string");

    double v = 0.0;
    check(as_units_convert(-20.0, AS_UNIT_DBFS, AS_UNIT_VRMS, 1.0,
                           std::sqrt(2.0), &v) == AS_OK &&
              std::fabs(v - 0.1) < 1e-9,
          "dBFS -> Vrms");
    check(as_units_convert(-20.0, AS_UNIT_DBFS, AS_UNIT_DBU, 1.0,
                           std::sqrt(2.0), &v) == AS_OK &&
              std::fabs(v + 17.78) < 0.01,
          "dBFS -> dBu");
    check(as_units_convert(-20.0, AS_UNIT_DBFS, AS_UNIT_VPP, 1.0,
                           std::sqrt(2.0), &v) == AS_OK &&
              std::fabs(v - 0.282842712) < 1e-6,
          "dBFS -> Vpp");

    AsSafetyGuard *g = as_safety_create();
    check(g != nullptr, "safety create");
    char reason[160];
    check(as_safety_precheck(g, 3, 1.0, 0, reason, sizeof(reason)) == AS_OK,
          "precheck low voltage without front end");
    check(as_safety_arm(g) == AS_OK, "arm");
    check(as_safety_start(g) == AS_OK, "start");
    check(as_safety_clip(g, 0, 1.02) == AS_OK &&
              as_safety_state(g) == AS_SAFETY_FAULT,
          "clip latch");
    check(as_safety_reset(g) == AS_OK && as_safety_state(g) == AS_SAFETY_IDLE,
          "reset");
    check(as_safety_precheck(g, 3, 10.0, 0, reason, sizeof(reason)) == AS_ERR_RANGE,
          "block 10 Vpp without front end");
    as_safety_destroy(g);

    namespace fs = std::filesystem;
    fs::path tmp = fs::temp_directory_path() / "audioscope-alpha-selftest";
    std::error_code ec;
    fs::remove_all(tmp, ec);
    fs::create_directories(tmp, ec);

    AsCalibration cal;
    as_calibration_init(&cal);
    std::snprintf(cal.device_id, sizeof(cal.device_id), "%s", "test-device");
    cal.fs_volts_rms = 1.0;
    cal.confidence = AS_CONF_MEASURED;
    as_calibration_seal(&cal);
    std::string cal_path = (tmp / "cal.json").string();
    check(as_calibration_write_json(&cal, cal_path.c_str()) == AS_OK,
          "calibration write");
    AsCalibration loaded;
    check(as_calibration_read_json(&loaded, cal_path.c_str()) == AS_OK,
          "calibration read");
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

    std::vector<float> sig(kFrames);
    AsGenerator gen;
    as_generator_init(&gen, kSampleRate);
    as_generator_sine(&gen, 1000.0, 0.5, sig.data(), kFrames);
    AsStats st;
    as_stats_compute(sig.data(), kFrames, kSampleRate, &st);
    check(std::fabs(st.freq - 1000.0) < 2.0, "stats frequency");

    AsFft fft;
    check(as_fft_init(&fft, 4096) == 0, "fft init");
    std::vector<double> x(4096), win(4096), mag(2049);
    AsGenerator gen2;
    as_generator_init(&gen2, kSampleRate);
    std::vector<float> sig2(4096);
    as_generator_sine(&gen2, 1000.0, 0.5, sig2.data(), 4096);
    for (int i = 0; i < 4096; ++i) x[(size_t)i] = (double)sig2[(size_t)i];
    as_window_hann(win.data(), 4096);
    for (int i = 0; i < 4096; ++i) x[(size_t)i] *= win[(size_t)i];
    as_fft_forward(&fft, x.data(), mag.data(), 4096);
    int peak = as_fft_peak_bin(mag.data(), 4096);
    check(std::abs(as_fft_bin_hz(peak, 4096, kSampleRate) - 1000.0) < 12.0,
          "fft 1 kHz peak");
    as_fft_free(&fft);

    AsciiCanvas canvas(20, 10);
    canvas.frame();
    std::string s_canvas = canvas.to_string();
    check(s_canvas.find('#') != std::string::npos, "ascii canvas");

    fs::remove_all(tmp, ec);
    std::printf("\n%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}

int cmd_scope() {
    std::vector<float> sig(kFrames);
    AsGenerator gen;
    as_generator_init(&gen, kSampleRate);
    as_generator_sine(&gen, 1000.0, 0.5, sig.data(), kFrames);
    AsStats st;
    std::string frame = render_scope(sig, kSampleRate, &st);
    std::printf("%s", frame.c_str());
    std::printf("Synthetic 1 kHz sine, 48 kHz, %.0f frames. Vpp %.3f V, %.1f Hz\n",
                (double)kFrames, st.vpp, st.freq);
    return 0;
}

int cmd_fft() {
    const int n = 4096;
    std::vector<float> sig(n);
    std::vector<double> x(n), win(n), mag(n / 2 + 1);
    AsGenerator gen;
    as_generator_init(&gen, kSampleRate);
    as_generator_sine(&gen, 1000.0, 0.5, sig.data(), n);
    for (int i = 0; i < n; ++i) x[(size_t)i] = (double)sig[(size_t)i];
    as_window_hann(win.data(), n);
    for (int i = 0; i < n; ++i) x[(size_t)i] *= win[(size_t)i];

    AsFft fft;
    if (as_fft_init(&fft, n) != 0) {
        std::fprintf(stderr, "fft init failed\n");
        return 1;
    }
    as_fft_forward(&fft, x.data(), mag.data(), n);
    int peak = as_fft_peak_bin(mag.data(), n);
    std::string frame = render_spectrum(mag, kSampleRate, peak);
    std::printf("%s", frame.c_str());
    std::printf("Synthetic 1 kHz sine, Hann 4096, peak bin %d = %.1f Hz\n",
                peak, as_fft_bin_hz(peak, n, kSampleRate));
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

int cmd_monitor(double seconds) {
    if (seconds <= 0.0) seconds = 5.0;
    if (seconds > 60.0) seconds = 60.0;
    AsAudio *audio = as_audio_open(-1, -2, kSampleRate, 128);
    if (!audio) {
        std::fprintf(stderr, "audio open failed: %s\n", as_audio_last_error());
        return 1;
    }
    std::printf("monitoring input for %.1f s; Ctrl-C to stop\n", seconds);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    std::vector<float> buf((size_t)kFrames * 2, 0.0f);
    std::vector<float> mono((size_t)kFrames, 0.0f);
    auto start = std::chrono::steady_clock::now();
    int frame_no = 0;
    enable_ansi();
    while (true) {
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - start).count();
        if (elapsed >= seconds) break;

        int got = as_audio_read(audio, buf.data(), kFrames);
        for (int i = 0; i < kFrames; ++i) {
            if (i < got) {
                mono[(size_t)i] = 0.5f * (buf[(size_t)i * 2] + buf[(size_t)i * 2 + 1]);
            } else {
                mono[(size_t)i] = 0.0f;
            }
        }
        AsStats st;
        std::string frame = render_scope(mono, as_audio_rate(audio), &st);
        std::printf("\x1b[2J\x1b[H");
        std::printf("%s", frame.c_str());
        std::printf("frame %d  %.1f s  |  alpha: no hardware protection\n",
                    frame_no++, elapsed);
        std::fflush(stdout);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    as_audio_close(audio);
    return 0;
}

int cmd_tone(double freq, double amp, double seconds) {
    if (freq < 20.0 || freq > 20000.0) {
        std::fprintf(stderr, "frequency must be 20..20000 Hz\n");
        return 1;
    }
    if (amp <= 0.0 || amp > 0.5) {
        std::fprintf(stderr, "amplitude must be 0..0.5 (linear)\n");
        return 1;
    }
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
    int rc = as_safety_precheck(g, 3, 1.0, 0, reason, sizeof(reason));
    std::printf("precheck 1 Vpp no FE: %s (%s)\n", as_status_string((AsStatus)rc), reason);
    as_safety_arm(g);
    as_safety_start(g);
    std::printf("running: %s\n", as_safety_state_name(as_safety_state(g)));
    as_safety_clip(g, 0, 1.02);
    std::printf("after clip: %s (%s)\n", as_safety_state_name(as_safety_state(g)),
                as_safety_reason(g));
    as_safety_reset(g);
    rc = as_safety_precheck(g, 3, 10.0, 0, reason, sizeof(reason));
    std::printf("precheck 10 Vpp no FE: %s (%s)\n", as_status_string((AsStatus)rc), reason);
    as_safety_destroy(g);
    return 0;
}

int cmd_session(const std::string &dir) {
    AsSession *s = as_session_create(dir.c_str(), "alpha-demo");
    if (!s) {
        std::fprintf(stderr, "session create failed\n");
        return 1;
    }
    as_session_event(s, "app_start", "{\"version\":\"0.1.0-alpha.1\"}");
    as_session_event(s, "input_mode", "{\"mode\":\"inst\"}");
    as_session_event(s, "note", "{\"text\":\"alpha session demo\"}");
    std::printf("session: %s (%d events)\n", as_session_path(s), as_session_count(s));
    as_session_destroy(s);
    return 0;
}

int cmd_calibrate(const std::string &path, double fs_volts, const std::string &device) {
    AsCalibration cal;
    as_calibration_init(&cal);
    std::snprintf(cal.device_id, sizeof(cal.device_id), "%s", device.c_str());
    cal.fs_volts_rms = fs_volts;
    cal.input_channel = 0;
    cal.confidence = AS_CONF_MEASURED;
    as_calibration_seal(&cal);
    int rc = as_calibration_write_json(&cal, path.c_str());
    if (rc != AS_OK) {
        std::fprintf(stderr, "calibration write failed: %s\n", as_status_string((AsStatus)rc));
        return 1;
    }
    std::printf("calibration written: %s (measured, %.3f Vrms FS)\n",
                path.c_str(), fs_volts);
    return 0;
}

double arg_num(int argc, char **argv, int index, double fallback) {
    if (index >= argc) return fallback;
    char *end = nullptr;
    double v = std::strtod(argv[index], &end);
    return end != argv[index] ? v : fallback;
}

} // namespace

int run_alpha_cli(int argc, char **argv) {
    enable_ansi();
    if (argc < 2) {
        print_help();
        return 0;
    }
    std::string cmd = argv[1];
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
    if (cmd == "scope") return cmd_scope();
    if (cmd == "fft") return cmd_fft();
    if (cmd == "units") return cmd_units();
    if (cmd == "safety") return cmd_safety();
    if (cmd == "monitor") return cmd_monitor(arg_num(argc, argv, 2, 5.0));
    if (cmd == "tone") {
        return cmd_tone(arg_num(argc, argv, 2, 1000.0),
                        arg_num(argc, argv, 3, 0.1),
                        arg_num(argc, argv, 4, 3.0));
    }
    if (cmd == "session") {
        return cmd_session(argc >= 3 ? argv[2] : "build/session");
    }
    if (cmd == "calibrate") {
        return cmd_calibrate(argc >= 3 ? argv[2] : "calibration.json",
                             arg_num(argc, argv, 3, 1.0),
                             argc >= 5 ? argv[4] : "default");
    }
    std::fprintf(stderr, "unknown command: %s\n\n", cmd.c_str());
    print_help();
    return 2;
}

} // namespace audioscope
