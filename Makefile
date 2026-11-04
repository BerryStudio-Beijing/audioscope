# AudioScope C — macOS build (clang, no third party libraries)
APP      := AudioScope
VERSION  := 1.1
BUILD    := build
SRC      := src
CFLAGS   := -std=c11 -O2 -Wall -Wextra -Isrc -mmacosx-version-min=12.0
OBJCFLAGS:= -fobjc-arc -O2 -Wall -mmacosx-version-min=12.0
FRAMEWORKS := -framework Cocoa -framework CoreAudio -framework AudioToolbox \
              -framework AudioUnit -framework CoreFoundation \
              -framework CoreGraphics -framework CoreText \
              -framework ImageIO -framework UniformTypeIdentifiers

CSRC := $(SRC)/dsp.c $(SRC)/audio.c $(SRC)/ui.c $(SRC)/theme.c $(SRC)/app.c \
        $(SRC)/view.c $(SRC)/wavegen.c $(SRC)/calib.c $(SRC)/analyze.c \
        $(SRC)/report.c $(SRC)/loopback.c tests/selftest.c tests/unit.c
MSRC := $(SRC)/main.m

OBJS := $(patsubst %.c,$(BUILD)/%.o,$(CSRC)) $(patsubst %.m,$(BUILD)/%.o,$(MSRC))

all: $(BUILD)/$(APP)

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.m
	@mkdir -p $(dir $@)
	$(CC) $(OBJCFLAGS) -c $< -o $@

# 测试文件固定用 -O1：本机 clang 在 -O2 + SLP 向量化下会对 selftest.c 的栈对象
# 生成越界写入（触发 stack canary abort），与被测代码无关；-O1 可稳定复现结果。
$(BUILD)/tests/%.o: tests/%.c
	@mkdir -p $(dir $@)
	$(CC) -std=c11 -O1 -Wall -Wextra -Isrc -c $< -o $@

$(BUILD)/$(APP): $(OBJS)
	$(CC) $(OBJS) -o $@ $(FRAMEWORKS) -lm -mmacosx-version-min=12.0

app: $(BUILD)/$(APP)
	@rm -rf $(BUILD)/$(APP).app
	@mkdir -p $(BUILD)/$(APP).app/Contents/MacOS
	@mkdir -p $(BUILD)/$(APP).app/Contents/Resources
	@cp $(BUILD)/$(APP) $(BUILD)/$(APP).app/Contents/MacOS/$(APP)
	@cp Info.plist $(BUILD)/$(APP).app/Contents/Info.plist
	@printf 'APPL????' > $(BUILD)/$(APP).app/Contents/PkgInfo
	@codesign --force --deep --sign - $(BUILD)/$(APP).app >/dev/null 2>&1 || true
	@echo "built $(BUILD)/$(APP).app"

run: all
	./$(BUILD)/$(APP)

dmg: app
	@rm -rf $(BUILD)/dmg_stage
	@mkdir -p $(BUILD)/dmg_stage
	@cp -R $(BUILD)/$(APP).app $(BUILD)/dmg_stage/
	@ln -s /Applications $(BUILD)/dmg_stage/Applications
	@cp 安装说明.txt $(BUILD)/dmg_stage/ 2>/dev/null || true
	@rm -f $(BUILD)/$(APP)-$(VERSION).dmg
	@hdiutil create -quiet -volname "$(APP) $(VERSION)" -srcfolder \
		$(BUILD)/dmg_stage -ov -format UDZO $(BUILD)/$(APP)-$(VERSION).dmg
	@echo "built $(BUILD)/$(APP)-$(VERSION).dmg"

test: all
	./$(BUILD)/$(APP) --selftest
	./$(BUILD)/$(APP) --unittest

# 发布产物：dist/ 里放带架构名的 DMG + ZIP + SHA256
release: app dmg
	@rm -rf dist && mkdir -p dist
	@cp $(BUILD)/$(APP)-$(VERSION).dmg \
		dist/$(APP)-$(VERSION)-macos-arm64.dmg
	@ditto -c -k --sequesterRsrc --keepParent $(BUILD)/$(APP).app \
		dist/$(APP)-$(VERSION)-macos-arm64.zip
	@cd dist && shasum -a 256 *.dmg *.zip > SHA256SUMS.txt
	@echo "--- dist ---" && ls -lh dist

shot: all
	@mkdir -p $(BUILD)/shots
	./$(BUILD)/$(APP) --shot $(BUILD)/shots

asan: CFLAGS += -fsanitize=address -g -O1
asan: OBJCFLAGS += -fsanitize=address -g -O1
asan: clean all
	ASAN_OPTIONS=detect_leaks=0 ./$(BUILD)/$(APP) --selftest

# Windows 命令行核心（无 GUI、无音频 IO）：纯 C，可用 mingw-w64 交叉编译
MINGW  ?= x86_64-w64-mingw32-gcc
WINOUT := dist/win

win-core:
	@mkdir -p $(WINOUT)
	$(MINGW) -std=c11 -O2 -Wall -Wextra -Isrc -D__USE_MINGW_ANSI_STDIO=1 \
		-o $(WINOUT)/$(APP)-$(VERSION)-win-x64-core.exe \
		src/dsp.c src/wavegen.c src/calib.c src/analyze.c src/report.c \
		tests/selftest.c tests/unit.c win/core_main.c -lm
	@file $(WINOUT)/$(APP)-$(VERSION)-win-x64-core.exe

clean:
	rm -rf $(BUILD)

.PHONY: all app run dmg test shot clean
