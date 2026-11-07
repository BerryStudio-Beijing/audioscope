/* AudioScope by BerryStudio 0.9 — native app entry point. */
#include "platform/macos/native_app.hpp"

int main(int argc, char **argv) {
    return audioscope::run_native_app(argc, argv);
}
