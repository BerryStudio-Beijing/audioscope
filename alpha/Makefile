# AudioScope by BerryStudio 0.8-alpha — macOS build + Windows cross-build.
#
# macOS:  make mac   -> build/audioscope-alpha
# Windows: make win  -> build/audioscope-alpha.exe (mingw-w64, WinMM)
# Tests:  make test  -> selftest + scope/fft smoke

BUILD := build
VERSION := $(shell cat VERSION)

MAC_CC ?= clang
MAC_CXX ?= clang++
WIN_CC ?= x86_64-w64-mingw32-gcc
WIN_CXX ?= x86_64-w64-mingw32-g++

COMMON_CFLAGS := -std=c11 -O2 -Wall -Wextra -Isrc -MMD -MP
COMMON_CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Isrc -MMD -MP
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
	src/core/pdf.c \
	src/core/phase.c \
	src/core/repair.c \
	src/core/spectrogram.c \
	src/core/tone.c \
	src/core/safety.c \
	src/core/calibration.c \
	src/core/report.c \
	src/core/session.c

APP_CXX := \
	src/ui/ascii_canvas.cpp \
	src/app/alpha_app.cpp \
	src/app/main.cpp

NATIVE_CXX := \
	src/app/native_model.cpp \
	src/ui/native_pages.cpp

NATIVE_C := \
	src/ui/native_ui.c \
	src/ui/native_theme.c

NATIVE_MM := \
	src/platform/macos/native_app.mm \
	src/platform/macos/native_main.mm

NATIVE_WIN_CXX := \
	src/app/native_model.cpp \
	src/platform/win/native_app_win.cpp

TEST_C := tests/test_ring.c

MAC_PLATFORM_C := \
	src/platform/macos/audio_coreaudio.c \
	src/platform/macos/devices.c

WIN_PLATFORM_C := \
	src/platform/win/audio_winmm.c \
	src/platform/win/devices_winmm.c

MAC_C := $(CORE_C) $(MAC_PLATFORM_C)
WIN_C := $(CORE_C) $(WIN_PLATFORM_C)

MAC_LIB_OBJS := $(patsubst %.c,$(BUILD)/mac/%.o,$(MAC_C))
MAC_OBJS := $(MAC_LIB_OBJS) \
            $(patsubst %.cpp,$(BUILD)/mac/%.o,$(APP_CXX))
NATIVE_OBJS := $(patsubst %.c,$(BUILD)/mac/%.o,$(NATIVE_C)) \
               $(patsubst %.cpp,$(BUILD)/mac/%.o,$(NATIVE_CXX)) \
               $(patsubst %.mm,$(BUILD)/mac/%.o,$(NATIVE_MM))
MAC_TEST_OBJS := $(patsubst %.c,$(BUILD)/mac/%.o,$(TEST_C)) \
                 $(BUILD)/mac/src/core/ring.o
NAV_TEST_OBJS := $(patsubst %.c,$(BUILD)/mac/%.o,tests/test_nav.c) \
                 $(BUILD)/mac/src/ui/native_ui.o \
                 $(BUILD)/mac/src/ui/native_theme.o
MODEL_TEST_OBJS := $(MAC_LIB_OBJS) $(BUILD)/mac/src/app/native_model.o \
                   $(BUILD)/mac/tests/test_model.o
WIN_LIB_OBJS := $(patsubst %.c,$(BUILD)/win/%.o,$(WIN_C))
WIN_OBJS := $(WIN_LIB_OBJS) \
            $(patsubst %.cpp,$(BUILD)/win/%.o,$(APP_CXX))
NATIVE_WIN_OBJS := $(patsubst %.cpp,$(BUILD)/win/%.o,$(NATIVE_WIN_CXX))

MAC_LDFLAGS := -framework CoreAudio -framework AudioToolbox -framework AudioUnit \
               -framework CoreFoundation -lpthread -lm
NATIVE_LDFLAGS := $(MAC_LDFLAGS) -framework Cocoa -framework CoreGraphics \
                  -framework CoreText -framework ImageIO -framework UniformTypeIdentifiers
OBJCFLAGS := -fobjc-arc -std=c++17 -O2 -Wall -Wextra -Isrc -mmacosx-version-min=12.0
WIN_LDFLAGS := -lwinmm -static -static-libgcc -static-libstdc++
NATIVE_WIN_LDFLAGS := -mwindows -lwinmm -lgdiplus -lshell32 -static -static-libgcc -static-libstdc++

-include $(MAC_OBJS:.o=.d) $(NATIVE_OBJS:.o=.d) $(WIN_OBJS:.o=.d) \
         $(MAC_TEST_OBJS:.o=.d) $(NAV_TEST_OBJS:.o=.d) $(MODEL_TEST_OBJS:.o=.d)

.PHONY: all mac app win win-app test clean dist

all: mac app

mac: $(BUILD)/audioscope-alpha

win: $(BUILD)/audioscope-alpha.exe

$(BUILD)/mac/%.o: %.c
	@mkdir -p $(dir $@)
	$(MAC_CC) $(MAC_CFLAGS) -c $< -o $@

$(BUILD)/mac/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(MAC_CXX) $(MAC_CXXFLAGS) -c $< -o $@

$(BUILD)/mac/%.o: %.mm
	@mkdir -p $(dir $@)
	$(MAC_CXX) $(OBJCFLAGS) -c $< -o $@

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

$(BUILD)/audioscope-alpha-navtest: $(NAV_TEST_OBJS)
	@mkdir -p $(BUILD)
	$(MAC_CXX) $(NAV_TEST_OBJS) -o $@ -framework CoreGraphics \
		-framework CoreText -framework CoreFoundation -lm

$(BUILD)/audioscope-alpha-modeltest: $(MODEL_TEST_OBJS)
	@mkdir -p $(BUILD)
	$(MAC_CXX) $(MODEL_TEST_OBJS) -o $@ $(MAC_LDFLAGS)

$(BUILD)/audioscope-alpha.exe: $(WIN_OBJS)
	@mkdir -p $(BUILD)
	$(WIN_CXX) $(WIN_OBJS) -o $@ $(WIN_LDFLAGS)

win-app: $(BUILD)/AudioScope.exe

$(BUILD)/AudioScope.exe: $(WIN_LIB_OBJS) $(NATIVE_WIN_OBJS)
	@mkdir -p $(BUILD)
	$(WIN_CXX) $(WIN_LIB_OBJS) $(NATIVE_WIN_OBJS) -o $@ $(NATIVE_WIN_LDFLAGS)

APP_BUNDLE := $(BUILD)/AudioScope.app
APP_BIN := $(APP_BUNDLE)/Contents/MacOS/AudioScope

app: $(APP_BIN)

$(APP_BIN): $(MAC_LIB_OBJS) $(NATIVE_OBJS)
	@mkdir -p $(APP_BUNDLE)/Contents/MacOS $(APP_BUNDLE)/Contents/Resources/fonts
	$(MAC_CXX) $(MAC_LIB_OBJS) $(NATIVE_OBJS) -o $@ $(NATIVE_LDFLAGS)
	@cp packaging/macos/Info.plist $(APP_BUNDLE)/Contents/Info.plist
	@cp assets/fonts/*.otf $(APP_BUNDLE)/Contents/Resources/fonts/
	@cp assets/fonts/OFL.txt $(APP_BUNDLE)/Contents/Resources/fonts/
	@printf 'APPL????' > $(APP_BUNDLE)/Contents/PkgInfo
	@codesign --force --deep --sign - $(APP_BUNDLE) >/dev/null 2>&1 || true
	@echo "built $(APP_BUNDLE)"

test: mac $(BUILD)/audioscope-alpha-ringtest $(BUILD)/audioscope-alpha-navtest \
      $(BUILD)/audioscope-alpha-modeltest app
	./$(BUILD)/audioscope-alpha selftest
	./$(BUILD)/audioscope-alpha scope > /dev/null
	./$(BUILD)/audioscope-alpha fft > /dev/null
	./$(BUILD)/audioscope-alpha-ringtest
	./$(BUILD)/audioscope-alpha-navtest
	./$(BUILD)/audioscope-alpha-modeltest
	rm -rf $(BUILD)/native-shots
	$(APP_BIN) --shot $(BUILD)/native-shots
	test -f $(BUILD)/native-shots/0_scope.png

dist: mac win
	rm -rf dist
	mkdir -p dist/macos dist/windows
	cp build/audioscope-alpha dist/macos/
	cp build/audioscope-alpha.exe dist/windows/
	cp README.md KNOWN-ISSUES.md CHANGELOG.md VERSION dist/macos/
	cp README.md KNOWN-ISSUES.md CHANGELOG.md VERSION dist/windows/
	cd dist/macos && zip -qr ../AudioScope-$(VERSION)-macos-arm64.zip .
	cd dist/windows && zip -qr ../AudioScope-$(VERSION)-windows-x64.zip .
	@ls -lh dist/*.zip

clean:
	rm -rf $(BUILD) dist
