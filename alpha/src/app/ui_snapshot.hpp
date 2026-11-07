/* AudioScope by BerryStudio 0.8-alpha — UI snapshot payload.
 *
 * The Web UI consumes exactly one object, `window.AUDIOSCOPE_DATA`. The same
 * builder feeds the static `ui` snapshot and the live WebSocket bridge, so the
 * two paths cannot drift apart.
 */
#ifndef AS_APP_UI_SNAPSHOT_HPP
#define AS_APP_UI_SNAPSHOT_HPP

#include "core/stats.h"
#include "core/tone.h"

#include <string>
#include <vector>

namespace audioscope {

struct UiSnapshotData {
    double fs = 48000.0;
    int channels = 2;
    int clip = 0;
    std::vector<float> scope;        /* 720 mono samples for the scope view */
    std::vector<double> spec_freqs;  /* 220 log-spaced frequencies, Hz */
    std::vector<double> spec_db;     /* matching dB values, peak-relative */
    AsStats stats{};
    AsToneResult tone{};
    double phase_deg = 45.0;
    double gain_db = 3.0;
    double drift_ppm = 0.0;
    const char *source = "synthetic snapshot";
    const char *device = "Studio 1824c";
    const char *mode = "Inst";
    const char *platform = "macOS · CoreAudio";
    double timestamp = 0.0; /* unix seconds; 0 = now */
};

/* 220 log-spaced bins from 20 Hz to 20 kHz, peak-relative, floored at -100 dB. */
void ui_spectrum_from_mag(const double *mag, int n, double fs,
                          std::vector<double> *freqs, std::vector<double> *db);

std::string ui_snapshot_json(const UiSnapshotData &data);

} // namespace audioscope

#endif
