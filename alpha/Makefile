# AudioScope by BerryStudio 0.2-alpha — macOS build + Windows cross-build.
#
# macOS:  make mac   -> build/audioscope-alpha
# Windows: make win  -> build/audioscope-alpha.exe (mingw-w64, WinMM)
# Tests:  make test  -> selftest + scope/fft smoke

BUILD := build

MAC_CC ?= clang
MAC_CXX ?= clang++
WIN_CC ?= x86_64-w64-mingw32-gcc
WIN_CXX ?= x86_64-w64-mingw32-g++

COMMON_CFLAGS := -std=c11 -O2 -Wall -Wextra -Isrc
COMMON_CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Isrc
MAC_CFLAGS := $(COMMON_CFLAGS) -mmacosx-version-min=12.0
MAC_CXXFLAGS := $(COMMON_CXXFLAGS) -mmacosx-version-min=12.0
WIN_CFLAGS := $(COMMON_CFLAGS) -D_WIN32_WINNT=0x0601
WIN_CXXFLAGS := $(COMMON_CXXFLAGS) -D_WIN32_WINNT=0x0601

CORE_C := \
	src/core/version.c \
	src/core/units.c \
	src/core/ring.c \
	src/core/fft.c \
	src/core/window.c \
	src/core/stats.c \
	src/core/generator.c \
	src/core/input_mode.c \
	src/core/safety.c \
	src/core/calibration.c \
	src/core/report.c \
	src/core/session.c

APP_CXX := \
	src/ui/ascii_canvas.cpp \
	src/app/alpha_app.cpp \
	src/app/main.cpp

TEST_C := tests/test_ring.c

MAC_PLATFORM_C := \
	src/platform/macos/audio_coreaudio.c \
	src/platform/macos/devices.c

WIN_PLATFORM_C := \
	src/platform/win/audio_winmm.c \
	src/platform/win/devices_winmm.c

MAC_C := $(CORE_C) $(MAC_PLATFORM_C)
WIN_C := $(CORE_C) $(WIN_PLATFORM_C)

MAC_OBJS := $(patsubst %.c,$(BUILD)/mac/%.o,$(MAC_C)) \
            $(patsubst %.cpp,$(BUILD)/mac/%.o,$(APP_CXX))
MAC_TEST_OBJS := $(patsubst %.c,$(BUILD)/mac/%.o,$(TEST_C)) \
                 $(BUILD)/mac/src/core/ring.o
WIN_OBJS := $(patsubst %.c,$(BUILD)/win/%.o,$(WIN_C)) \
            $(patsubst %.cpp,$(BUILD)/win/%.o,$(APP_CXX))

MAC_LDFLAGS := -framework CoreAudio -framework AudioToolbox -framework AudioUnit \
               -framework CoreFoundation -lpthread -lm
WIN_LDFLAGS := -lwinmm -static -static-libgcc -static-libstdc++

.PHONY: all mac win test clean dist

all: mac

mac: $(BUILD)/audioscope-alpha

win: $(BUILD)/audioscope-alpha.exe

$(BUILD)/mac/%.o: %.c
	@mkdir -p $(dir $@)
	$(MAC_CC) $(MAC_CFLAGS) -c $< -o $@

$(BUILD)/mac/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(MAC_CXX) $(MAC_CXXFLAGS) -c $< -o $@

$(BUILD)/win/%.o: %.c
	@mkdir -p $(dir $@)
	$(WIN_CC) $(WIN_CFLAGS) -c $< -o $@

$(BUILD)/win/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(WIN_CXX) $(WIN_CXXFLAGS) -c $< -o $@

$(BUILD)/audioscope-alpha: $(MAC_OBJS)
	@mkdir -p $(BUILD)
	$(MAC_CXX) $(MAC_OBJS) -o $@ $(MAC_LDFLAGS)

$(BUILD)/audioscope-alpha-ringtest: $(MAC_TEST_OBJS)
	@mkdir -p $(BUILD)
	$(MAC_CXX) $(MAC_TEST_OBJS) -o $@ $(MAC_LDFLAGS)

$(BUILD)/audioscope-alpha.exe: $(WIN_OBJS)
	@mkdir -p $(BUILD)
	$(WIN_CXX) $(WIN_OBJS) -o $@ $(WIN_LDFLAGS)

test: mac $(BUILD)/audioscope-alpha-ringtest
	./$(BUILD)/audioscope-alpha selftest
	./$(BUILD)/audioscope-alpha scope > /dev/null
	./$(BUILD)/audioscope-alpha fft > /dev/null
	./$(BUILD)/audioscope-alpha-ringtest

dist: mac win
	rm -rf dist
	mkdir -p dist/macos dist/windows
	cp build/audioscope-alpha dist/macos/
	cp build/audioscope-alpha.exe dist/windows/
	cp README.md KNOWN-ISSUES.md CHANGELOG.md VERSION dist/macos/
	cp README.md KNOWN-ISSUES.md CHANGELOG.md VERSION dist/windows/
	cd dist/macos && zip -qr ../AudioScope-0.2.0-alpha.1-macos-arm64.zip .
	cd dist/windows && zip -qr ../AudioScope-0.2.0-alpha.1-windows-x64.zip .
	@ls -lh dist/*.zip

clean:
	rm -rf $(BUILD) dist
