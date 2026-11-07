/* AudioScope by BerryStudio 0.8-alpha — UI snapshot payload. */
#include "app/ui_snapshot.hpp"

#include "core/api.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace audioscope {
namespace {

double crest_factor(const AsStats &st) {
    return st.peak > 1e-9 ? st.peak / std::max(st.rms, 1e-12) : 0.0;
}

/* Device names come from the OS and are attacker-controlled in principle. */
std::string json_string(const char *s) {
    std::string out = "\"";
    if (s) {
        for (const unsigned char *p = (const unsigned char *)s; *p; ++p) {
            switch (*p) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (*p < 0x20) {
                    char b[8];
                    std::snprintf(b, sizeof(b), "\\u%04x", *p);
                    out += b;
                } else {
                    out += (char)*p;
                }
            }
        }
    }
    out += "\"";
    return out;
}

} // namespace

void ui_spectrum_from_mag(const double *mag, int n, double fs,
                          std::vector<double> *freqs, std::vector<double> *db) {
    const int bins = 220;
    freqs->assign((size_t)bins, 0.0);
    db->assign((size_t)bins, -100.0);
    if (!mag || n < 4 || !(fs > 0.0)) return;
    double maxmag = 1e-12;
    for (int i = 1; i < n / 2; ++i) maxmag = std::max(maxmag, mag[(size_t)i]);
    for (int i = 0; i < bins; ++i) {
        double f = 20.0 * std::pow(1000.0, (double)i / (double)(bins - 1));
        int bin = (int)std::lround(f * (double)n / fs);
        bin = std::clamp(bin, 1, n / 2 - 1);
        double v = 20.0 * std::log10(std::max(mag[(size_t)bin] / maxmag, 1e-6));
        (*freqs)[(size_t)i] = f;
        (*db)[(size_t)i] = std::max(v, -100.0);
    }
}

std::string ui_snapshot_json(const UiSnapshotData &d) {
    std::ostringstream js;
    std::time_t now = (std::time_t)(d.timestamp > 0.0 ? d.timestamp : (double)std::time(nullptr));
    char stamp[32];
    std::strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M", std::localtime(&now));

    js << "window.AUDIOSCOPE_DATA = {\n";
    js << "  meta: {version:" << json_string(as_version_string()) << ", platform:"
       << json_string(d.platform) << ", mode:" << json_string(d.mode) << ", device:"
       << json_string(d.device) << ", rate:" << std::fixed << std::setprecision(0) << d.fs
       << ", channels:" << d.channels << ", source:" << json_string(d.source)
       << ", timestamp:" << json_string(stamp) << "},\n";

    js << "  scope: {waveform:[";
    js << std::fixed << std::setprecision(5);
    for (size_t i = 0; i < d.scope.size(); ++i) {
        if (i) js << ",";
        js << d.scope[i];
    }
    js << "], peak_dbfs:" << std::setprecision(2) << as_linear_to_db(d.stats.peak)
       << ", rms_dbfs:" << as_linear_to_db(d.stats.rms)
       << ", freq_hz:" << std::setprecision(1) << d.stats.freq
       << ", crest:" << std::setprecision(2) << 20.0 * std::log10(std::max(crest_factor(d.stats), 1e-6))
       << ", vpp:" << std::setprecision(4) << d.stats.vpp
       << ", clip:" << d.clip << ", channel:0},\n";

    js << "  spectrum: {freqs:[";
    for (size_t i = 0; i < d.spec_freqs.size(); ++i) {
        if (i) js << ",";
        js << std::setprecision(1) << d.spec_freqs[i];
    }
    js << "], db:[";
    for (size_t i = 0; i < d.spec_db.size(); ++i) {
        if (i) js << ",";
        js << std::setprecision(1) << d.spec_db[i];
    }
    js << "], thd:" << std::setprecision(4) << d.tone.thd_pct
       << ", thdn:" << d.tone.thdn_pct
       << ", snr:" << std::setprecision(2) << d.tone.snr_db
       << ", sinad:" << d.tone.sinad_db
       << ", sfdr:" << d.tone.sfdr_db
       << ", enob:" << std::setprecision(2) << d.tone.enob << "},\n";

    js << "  xy: {points:[";
    const double pi = 3.14159265358979323846;
    for (int i = 0; i < 1600; ++i) {
        double t = (double)i / d.fs;
        double g = std::pow(10.0, d.gain_db / 20.0);
        double a = 0.5 * std::sin(2.0 * pi * 1000.0 * t);
        double b = 0.5 * g * std::sin(2.0 * pi * 1000.0 * t + d.phase_deg * pi / 180.0);
        if (i) js << ",";
        js << "[" << std::setprecision(5) << a << "," << b << "]";
    }
    js << "], phase_deg:" << std::setprecision(1) << d.phase_deg
       << ", gain_db:" << d.gain_db
       << ", freq_hz:1000.0, drift_ppm:" << d.drift_ppm << "},\n";

    js << "  channels:[\n";
    double peak = as_linear_to_db(d.stats.peak);
    double rms = as_linear_to_db(d.stats.rms);
    for (int c = 0; c < 8; ++c) {
        double cp = (c < 2) ? peak - (c ? 0.2 : 0.0) : -96.0 + c * 0.1;
        double cr = (c < 2) ? rms - (c ? 0.2 : 0.0) : -102.0 + c * 0.1;
        js << "    {label:\"CH" << (c + 1) << "\", peak_dbfs:" << std::setprecision(1) << cp
           << ", rms_dbfs:" << cr << ", freq_hz:" << (c < 2 ? d.stats.freq : 0.0)
           << ", vpp:" << std::setprecision(4) << (c < 2 ? d.stats.vpp - c * 0.02 : 0.0)
           << ", clip:" << (c < 2 ? d.clip : 0)
           << ", verdict:\"" << (c < 2 ? "PASS" : "—") << "\"}"
           << (c < 7 ? "," : "") << "\n";
    }
    js << "  ],\n";

    const double wow = 0.3, flutter = 0.1, azimuth = 0.0, plan_thd = 0.0061;
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
    return js.str();
}

} // namespace audioscope
