/* Regression test: model-level trust invariants.
 *
 * 1.0 的关键承诺：CH1/CH2 独立统计、Tape Plan 永远标记为模拟、FFT 尺寸
 * 只接受 2 的幂、时间累计使用模型实际采样率。
 */
#include "app/native_model.hpp"

#include <cmath>
#include <cstdio>

namespace {

int g_fail = 0;

void check(bool cond, const char *what) {
    std::printf("  %s  %s\n", cond ? "ok  " : "fail", what);
    if (!cond) ++g_fail;
}

} // namespace

int main() {
    audioscope::NativeEngine engine;
    audioscope::NativeModel model;
    for (int i = 0; i < 4; ++i) engine.tick(&model, 1.0 / 30.0);

    check(model.ch[0].active, "CH1 active");
    check(model.ch[1].active, "CH2 active");
    const double ch_delta = model.ch[0].rms_db - model.ch[1].rms_db;
    check(std::fabs(ch_delta - 0.915) < 0.3,
          "CH2 independent (0.9 amplitude => ~0.92 dB)");
    check(model.ch[0].stats.rms > 0.0 && model.ch[1].stats.rms > 0.0,
          "per-channel stats present");

    engine.run_repair(&model);
    check(model.plan_ran && model.plan_simulated, "Tape Plan is simulated");
    check(!model.plan.empty(), "Tape Plan has steps");
    check(model.message.find("simulated") != std::string::npos,
          "simulated plan message");

    check(engine.set_fft_size(&model, 16384) && model.fft_size == 16384,
          "FFT 16384 accepted");
    check(!engine.set_fft_size(&model, 1000) && model.fft_size == 16384,
          "non power-of-two rejected");

    const double before = model.seconds;
    model.fs = 44100.0;
    engine.tick(&model, 1.0 / 30.0);
    const double expected = 720.0 / 44100.0;
    check(std::fabs((model.seconds - before) - expected) < 1e-6,
          "seconds use actual sample rate");

    std::printf("%d failure(s)\n", g_fail);
    return g_fail == 0 ? 0 : 1;
}
