/* AudioScope by BerryStudio 0.9 — native macOS application.
 *
 * AppKit shell + CoreGraphics view. No WebView, no HTML, no local HTTP.
 */
#include "ui/native_theme.h"

#import <Cocoa/Cocoa.h>
#import <CoreText/CoreText.h>
#import <ImageIO/ImageIO.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include "app/native_model.hpp"
#include "platform/macos/native_app.hpp"
#include "ui/native_pages.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

using namespace audioscope;

namespace {

NativeEngine g_engine;
NativeModel g_model;
NativeUi g_ui;
NativeActions g_pending;

double g_view_scale = 1.0;
double g_view_ox = 0.0;
double g_view_oy = 0.0;

/* 单调时钟：动画与引擎都用真实经过时间，不再用"帧数/30"。
 * 这样刷新率变化时动画时长不变（0.9 的导航指示线就是被帧率拖慢的）。 */
double now_seconds() {
    using clock = std::chrono::steady_clock;
    static const clock::time_point t0 = clock::now();
    return std::chrono::duration<double>(clock::now() - t0).count();
}

double g_anim_t = 0.0;

constexpr double kDesignW = 1120.0;
constexpr double kDesignH = 700.0;
constexpr double kFrameHz = 60.0;

void register_font_file(const char *path) {
    if (!path || !*path) return;
    NSString *s = [NSString stringWithUTF8String:path];
    if (![[NSFileManager defaultManager] fileExistsAtPath:s]) return;
    NSURL *url = [NSURL fileURLWithPath:s];
    CTFontManagerRegisterFontsForURL((__bridge CFURLRef)url,
                                     kCTFontManagerScopeProcess, NULL);
}

void register_fonts() {
    NSArray<NSString *> *names = @[
        @"Geist-Regular", @"Geist-Medium", @"Geist-Bold", @"GeistMono-Regular"
    ];
    for (NSString *name in names) {
        NSString *p = [[NSBundle mainBundle] pathForResource:name
                                                      ofType:@"otf"
                                                 inDirectory:@"fonts"];
        if (p) register_font_file(p.fileSystemRepresentation);
    }
    if ([[NSBundle mainBundle] pathForResource:@"Geist-Regular" ofType:@"otf"
                                   inDirectory:@"fonts"])
        return;
    /* Running the bare executable during development. */
    const char *bases[] = {"alpha/assets/fonts", "assets/fonts", "../assets/fonts"};
    const char *files[] = {"Geist-Regular.otf", "Geist-Medium.otf",
                           "Geist-Bold.otf", "GeistMono-Regular.otf"};
    for (const char *base : bases) {
        std::string probe = std::string(base) + "/" + files[0];
        if (![[NSFileManager defaultManager] fileExistsAtPath:
                  [NSString stringWithUTF8String:probe.c_str()]])
            continue;
        for (const char *f : files) {
            std::string p = std::string(base) + "/" + f;
            register_font_file(p.c_str());
        }
        break;
    }
}

int render_png(const char *path, int scale) {
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx = CGBitmapContextCreate(
        NULL, (size_t)(kDesignW * scale), (size_t)(kDesignH * scale), 8,
        (size_t)(kDesignW * scale) * 4, cs,
        (CGBitmapInfo)kCGImageAlphaPremultipliedLast);
    if (!ctx) {
        CGColorSpaceRelease(cs);
        return -1;
    }
    CGContextTranslateCTM(ctx, 0, kDesignH * scale);
    CGContextScaleCTM(ctx, scale, -scale);
    native_ui_frame(&g_ui, ctx, kDesignW, kDesignH, g_anim_t);
    CGImageRef img = CGBitmapContextCreateImage(ctx);
    CGContextRelease(ctx);
    CGColorSpaceRelease(cs);
    if (!img) return -1;
    NSString *p = [NSString stringWithUTF8String:path];
    NSURL *url = [NSURL fileURLWithPath:p];
    int rc = -1;
    for (int attempt = 0; attempt < 2 && rc != 0; ++attempt) {
        CGImageDestinationRef dst = CGImageDestinationCreateWithURL(
            (__bridge CFURLRef)url, (__bridge CFStringRef)UTTypePNG.identifier, 1, NULL);
        if (!dst) continue;
        CGImageDestinationAddImage(dst, img, NULL);
        rc = CGImageDestinationFinalize(dst) ? 0 : -1;
        CFRelease(dst);
    }
    CGImageRelease(img);
    return rc;
}

int run_shots(const char *dir, bool light) {
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    register_fonts();
    native_ui_init(&g_ui);
    g_ui.model = &g_model;
    g_reduce_motion = 1;
    theme_set(light ? THEME_LIGHT : THEME_DARK);
    g_model.light = light;
    for (int i = 0; i < 20; ++i) g_engine.tick(&g_model, 1.0 / 30.0);
    g_engine.run_repair(&g_model);
    static const char *names[PAGE_COUNT] = {"scope", "spectrum", "xy", "measure",
                                            "repair", "reports", "calibration"};
    for (int i = 0; i < PAGE_COUNT; ++i) {
        g_model.page = i;
        char path[512];
        std::snprintf(path, sizeof(path), "%s/%d_%s.png", dir, i, names[i]);
        if (render_png(path, 2) != 0) {
            std::fprintf(stderr, "failed to write %s\n", path);
            return 1;
        }
    }
    std::printf("native screenshots written to %s\n", dir);
    return 0;
}

void show_alert(const char *title, const char *text, bool warning) {
    NSAlert *alert = [[NSAlert alloc] init];
    alert.messageText = [NSString stringWithUTF8String:title];
    alert.informativeText = [NSString stringWithUTF8String:text ? text : ""];
    alert.alertStyle = warning ? NSAlertStyleWarning : NSAlertStyleInformational;
    [alert addButtonWithTitle:@"OK"];
    [alert runModal];
}

void handle_capture() {
    if (g_model.capture) {
        g_engine.set_capture(&g_model, false, nullptr);
        g_model.message = "capture stopped";
        return;
    }
    NSAlert *alert = [[NSAlert alloc] init];
    alert.messageText = @"Switch to live input?";
    alert.informativeText =
        @"AudioScope will open the default input device and may ask for microphone "
        @"permission. The UI is currently showing synthetic data. Stop capture at "
        @"any time; the input mode is never switched automatically.";
    alert.alertStyle = NSAlertStyleWarning;
    [alert addButtonWithTitle:@"Capture"];
    [alert addButtonWithTitle:@"Cancel"];
    if ([alert runModal] != NSAlertFirstButtonReturn) return;
    std::string err;
    if (!g_engine.set_capture(&g_model, true, &err)) {
        show_alert("Cannot open input", err.empty() ? "unknown error" : err.c_str(), true);
        return;
    }
    g_model.message = "capturing input";
}

void handle_loopback() {
    NSAlert *alert = [[NSAlert alloc] init];
    alert.messageText = @"Run a loopback measurement?";
    alert.informativeText =
        @"A −20 dBFS 1 kHz tone will play on the default output and be recorded "
        @"from the default input. Route output back to input (or use a loopback "
        @"device) before continuing. Software cannot protect the sound card.";
    alert.alertStyle = NSAlertStyleWarning;
    [alert addButtonWithTitle:@"Run 2 s"];
    [alert addButtonWithTitle:@"Cancel"];
    if ([alert runModal] != NSAlertFirstButtonReturn) return;
    std::string err;
    if (!g_engine.run_loopback(&g_model, &err)) {
        show_alert("Loopback failed", err.empty() ? "no frames captured" : err.c_str(), true);
        return;
    }
    char msg[256];
    std::snprintf(msg, sizeof(msg), "Level %.1f dBFS · THD %.3f %% · %s",
                  g_model.loopback_level, g_model.loopback_thd,
                  g_model.loopback_pass ? "connectivity OK" : "connectivity FAIL");
    show_alert("Loopback result", msg, !g_model.loopback_pass);
}

void handle_export() {
    std::string path, err;
    if (!g_engine.export_report(&g_model, &path, &err)) {
        show_alert("Export failed", err.c_str(), true);
        return;
    }
    NSString *msg = [NSString stringWithFormat:@"Report written to:\n%s", path.c_str()];
    NSAlert *alert = [[NSAlert alloc] init];
    alert.messageText = @"Report written";
    alert.informativeText = msg;
    alert.alertStyle = NSAlertStyleInformational;
    [alert addButtonWithTitle:@"Open PDF"];
    [alert addButtonWithTitle:@"Close"];
    if ([alert runModal] == NSAlertFirstButtonReturn)
        [[NSWorkspace sharedWorkspace] openURL:[NSURL fileURLWithPath:[NSString stringWithUTF8String:path.c_str()]]];
}

void process_pending() {
    if (g_pending.run_repair) {
        g_pending.run_repair = false;
        g_engine.run_repair(&g_model);
    }
    if (g_pending.toggle_capture) {
        g_pending.toggle_capture = false;
        handle_capture();
    }
    if (g_pending.export_report) {
        g_pending.export_report = false;
        handle_export();
    }
    if (g_pending.run_loopback) {
        g_pending.run_loopback = false;
        handle_loopback();
    }
}

} // namespace

@interface ASNativeView : NSView
- (void)update_metrics;
@end

@implementation ASNativeView
- (BOOL)isFlipped { return YES; }
- (BOOL)acceptsFirstResponder { return YES; }

/* 等比 letterbox：窗口任意比例都按 min(w/1120, h/700) 缩放并居中。
 * 0.9 只在 drawRect 里算一次，窗口一改大小鼠标映射就还是旧值；
 * 现在几何只在 update_metrics 里算，绘制和命中测试共用同一份。 */
- (void)update_metrics {
    NSRect b = self.bounds;
    double s = std::min(b.size.width / kDesignW, b.size.height / kDesignH);
    if (!(s > 0.0)) s = 1.0;
    g_view_scale = s;
    g_view_ox = (b.size.width - kDesignW * s) * 0.5;
    g_view_oy = (b.size.height - kDesignH * s) * 0.5;
}

- (void)setFrameSize:(NSSize)newSize {
    [super setFrameSize:newSize];
    [self update_metrics];
    [self setNeedsDisplay:YES];
}

- (void)viewDidMoveToWindow {
    [super viewDidMoveToWindow];
    self.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    [self update_metrics];
}

- (void)drawRect:(NSRect)dirty {
    (void)dirty;
    NSGraphicsContext *gc = [NSGraphicsContext currentContext];
    if (!gc) return;
    CGContextRef ctx = gc.CGContext;
    NSRect b = self.bounds;
    [self update_metrics];

    CGContextSaveGState(ctx);
    /* letterbox 外边跟主题同色，不再是一条生硬的黑边。 */
    CGContextSetFillColorWithColor(
        ctx, [[NSColor colorWithCalibratedRed:((g_pal.bg >> 24) & 0xff) / 255.0
                                        green:((g_pal.bg >> 16) & 0xff) / 255.0
                                         blue:((g_pal.bg >> 8) & 0xff) / 255.0
                                        alpha:1.0] CGColor]);
    CGContextFillRect(ctx, b);
    CGContextSetShouldAntialias(ctx, true);
    CGContextSetAllowsAntialiasing(ctx, true);
    CGContextSetShouldSmoothFonts(ctx, true);
    CGContextTranslateCTM(ctx, g_view_ox, g_view_oy);
    CGContextScaleCTM(ctx, g_view_scale, g_view_scale);
    native_ui_frame(&g_ui, ctx, kDesignW, kDesignH, g_anim_t);
    CGContextRestoreGState(ctx);

    if (g_ui.actions.run_repair) g_pending.run_repair = true;
    if (g_ui.actions.toggle_capture) g_pending.toggle_capture = true;
    if (g_ui.actions.export_report) g_pending.export_report = true;
    if (g_ui.actions.run_loopback) g_pending.run_loopback = true;
}

- (NSPoint)design_point:(NSEvent *)e {
    NSPoint p = [self convertPoint:e.locationInWindow fromView:nil];
    return NSMakePoint((p.x - g_view_ox) / g_view_scale,
                       (p.y - g_view_oy) / g_view_scale);
}

- (void)mouseDown:(NSEvent *)e {
    NSPoint p = [self design_point:e];
    native_ui_mouse_down(&g_ui, p.x, p.y);
    [self setNeedsDisplay:YES];
}
- (void)mouseDragged:(NSEvent *)e {
    NSPoint p = [self design_point:e];
    native_ui_mouse_move(&g_ui, p.x, p.y);
    [self setNeedsDisplay:YES];
}
- (void)mouseUp:(NSEvent *)e {
    NSPoint p = [self design_point:e];
    native_ui_mouse_up(&g_ui, p.x, p.y);
    [self setNeedsDisplay:YES];
}
- (void)keyDown:(NSEvent *)e {
    NSString *s = e.charactersIgnoringModifiers;
    if (s.length == 1) {
        unichar c = [s characterAtIndex:0];
        if (c >= '1' && c <= '7') {
            g_model.page = (int)(c - '1');
            [self setNeedsDisplay:YES];
            return;
        }
        if (c == ' ') {
            g_model.hold = !g_model.hold;
            [self setNeedsDisplay:YES];
            return;
        }
        if (c == 'c' || c == 'C') { g_pending.toggle_capture = true; return; }
        if (c == 'r' || c == 'R') { g_pending.run_repair = true; return; }
        if (c == 'e' || c == 'E') { g_pending.export_report = true; return; }
        if (c == 't' || c == 'T') {
            theme_toggle();
            g_model.light = theme_is_light();
            [self setNeedsDisplay:YES];
            return;
        }
    }
    [super keyDown:e];
}
@end

@interface ASAppDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate>
@property(nonatomic, strong) NSWindow *window;
@property(nonatomic, weak) NSView *view;
@end

@implementation ASAppDelegate
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)app {
    (void)app;
    return YES;
}

- (void)windowDidResize:(NSNotification *)note {
    (void)note;
    /* 拖动窗口边缘时立即按新比例重绘，而不是等下一次定时器。 */
    [self.view setNeedsDisplay:YES];
}
@end

static void install_menu() {
    NSMenu *menubar = [[NSMenu alloc] init];
    NSMenuItem *appItem = [[NSMenuItem alloc] init];
    [menubar addItem:appItem];
    [NSApp setMainMenu:menubar];
    NSMenu *appMenu = [[NSMenu alloc] init];
    [appMenu addItemWithTitle:@"About AudioScope"
                       action:@selector(orderFrontStandardAboutPanel:)
                keyEquivalent:@""];
    [appMenu addItem:[NSMenuItem separatorItem]];
    [appMenu addItemWithTitle:@"Quit AudioScope"
                       action:@selector(terminate:)
                keyEquivalent:@"q"];
    appItem.submenu = appMenu;
}

namespace audioscope {

int run_native_app(int argc, char **argv) {
    @autoreleasepool {
        const char *shot_dir = nullptr;
        bool light = false;
        bool export_test = false;
        for (int i = 1; i < argc; ++i) {
            if (std::strcmp(argv[i], "--shot") == 0 && i + 1 < argc) shot_dir = argv[++i];
            else if (std::strcmp(argv[i], "--light") == 0) light = true;
            else if (std::strcmp(argv[i], "--export-test") == 0) export_test = true;
        }
        if (shot_dir) return run_shots(shot_dir, light);

        register_fonts();
        native_ui_init(&g_ui);
        g_ui.model = &g_model;
        for (int i = 0; i < 12; ++i) g_engine.tick(&g_model, 1.0 / 30.0);
        if (export_test) {
            g_engine.run_repair(&g_model);
            std::string path, err;
            if (!g_engine.export_report(&g_model, &path, &err)) {
                std::fprintf(stderr, "export failed: %s\n", err.c_str());
                return 1;
            }
            std::printf("export ok: %s\n", path.c_str());
            return 0;
        }

        NSApplication *app = [NSApplication sharedApplication];
        [app setActivationPolicy:NSApplicationActivationPolicyRegular];
        install_menu();

        ASAppDelegate *delegate = [[ASAppDelegate alloc] init];
        app.delegate = delegate;

        NSRect frame = NSMakeRect(0, 0, kDesignW, kDesignH);
        NSWindow *win = [[NSWindow alloc]
            initWithContentRect:frame
                      styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable
                        backing:NSBackingStoreBuffered
                          defer:NO];
        win.title = @"AudioScope by BerryStudio";
        win.minSize = NSMakeSize(900, 560);
        win.backgroundColor = [NSColor blackColor];
        ASNativeView *view = [[ASNativeView alloc] initWithFrame:frame];
        view.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        win.contentView = view;
        win.delegate = delegate;
        delegate.view = view;
        [win center];
        [win makeKeyAndOrderFront:nil];
        [win makeFirstResponder:view];
        delegate.window = win;

        /* 60 Hz 刷新 + 真实 dt：帧率不再是动画速度的一部分。
         * commonModes 保证拖动窗口 / 打开菜单时动画不冻结。 */
        __block double frame_last = now_seconds();
        NSTimer *timer = [NSTimer timerWithTimeInterval:1.0 / kFrameHz
                                               repeats:YES
                                                 block:^(NSTimer *t) {
            (void)t;
            double now = now_seconds();
            double dt = now - frame_last;
            frame_last = now;
            if (dt > 0.25) dt = 0.25;   /* 跟踪循环结束后不要一次补太多 */
            if (dt < 0.0) dt = 0.0;
            g_anim_t = now;
            g_engine.tick(&g_model, dt);
            process_pending();
            [view setNeedsDisplay:YES];
        }];
        [[NSRunLoop currentRunLoop] addTimer:timer forMode:NSRunLoopCommonModes];

        [app activateIgnoringOtherApps:YES];
        [app run];
    }
    return 0;
}

} // namespace audioscope
