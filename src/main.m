// AudioScope C — macOS shell (AppKit). All DSP/audio/UI drawing is plain C.
#import <Cocoa/Cocoa.h>
#import <CoreGraphics/CoreGraphics.h>
#import <ImageIO/ImageIO.h>
#import <UniformTypeIdentifiers/UTType.h>
#include <sys/stat.h>
#include <unistd.h>

#include "app.h"

int run_selftest(void);
int run_unittest(void);
int run_simulate(const char *dir);
static int list_devices_cmd(void);

static App g_app;

/* ------------------------------------------------------------------ */
/* offscreen rendering (also used by --shot for regression screenshots) */
/* ------------------------------------------------------------------ */
static void demo_fill(App *a) {
    double fs = a->fs;
    a->running = 1;
    a->gen.wave = W_SQUARE;
    a->gen.freq = 1000.0;
    a->gen.amp_db = -6.0;
    a->gen.ch_en[0] = 1;
    a->gen.ch_en[1] = 1;
    a->time_div = 1e-3;
    a->volt_div = 0.2;
    a->scope.span = 0.01;
    a->scope.n = 480;
    for (int i = 0; i < a->scope.n; i++) {
        double t = a->scope.span * i / (a->scope.n - 1);
        a->scope.ch1[i] = (float)(0.5 * sin(2.0 * 3.14159265358979 * 1000.0 * t));
        a->scope.ch2[i] =
            (float)(0.22 * sin(2.0 * 3.14159265358979 * 1000.0 * t - 0.9));
        a->scope.math[i] = a->scope.ch1[i] - a->scope.ch2[i];
    }
    a->scope.has_math = 1;
    time_stats(a->scope.ch1, a->scope.n, 480.0 / a->scope.span, 1.0,
               &a->tstat[0]);
    time_stats(a->scope.ch2, a->scope.n, 480.0 / a->scope.span, 1.0,
               &a->tstat[1]);

    /* spectrum: fundamental + odd harmonics + noise floor */
    double fmin = 20.0, fmax = fs * 0.5;
    for (int j = 0; j < SPEC_COLS; j++) {
        double f = fmin * pow(fmax / fmin, (double)j / (SPEC_COLS - 1));
        double db = -119.0 + 6.0 * log10(j / 40.0 + 1.0);
        for (int k = 1; k <= 9; k += 2) {
            double fk = 1000.0 * k;
            double amp = -6.0 - 20.0 * log10((double)k);
            double d = (log(f) - log(fk)) / 0.02;
            db = AS_MAX(db, amp - 6.0 * d * d);
        }
        a->spec.db[0][j] = (float)db;
        a->spec.db[1][j] = (float)(db - 9.0);
        a->spec.db[2][j] = (float)(db + 1.5);
        a->spec_peak[j] = (float)(db + 1.5);
    }
    a->spec.mark_n = 5;
    for (int k = 0; k < 5; k++) {
        a->spec.marks_f[k] = 1000.0 * (2 * k + 1);
        a->spec.marks_db[k] = (float)(-6.0 - 20.0 * log10((double)(2 * k + 1)));
    }
    a->spec.n = SPEC_COLS;
    a->spec_low = 20.0;
    a->spec_high = fs * 0.5;
    a->tone[0].freq = 1000.0;
    a->tone[0].amp = 0.5;
    a->tone[0].amp_db = -6.02;
    a->tone[0].thd = 0.416;
    a->tone[0].thdn = 0.43;
    a->tone[0].snr = 19.0;
    a->tone[0].sinad = 7.3;
    a->tone[0].enob = 0.92;
    a->tone[0].sfdr = 25.2;
    a->tone_valid[0] = 1;
    a->tone_valid[1] = 1;
    a->tone[1] = a->tone[0];
    a->tstat[0].clipped = 0;

    /* waterfall: rising sweep */
    for (int r = 0; r < WF_ROWS; r++) {
        unsigned char *row = a->wf.rgba + (size_t)r * WF_COLS * 4;
        double f0 = 120.0 * pow(60.0, (double)r / WF_ROWS);
        for (int j = 0; j < WF_COLS; j++) {
            double f = fmin * pow(fmax / fmin, (double)j / (WF_COLS - 1));
            double d = (log(f) - log(f0)) / 0.05;
            double v = -112.0 + 4.0 * sin(j * 0.05) + 2.0 * cos(r * 0.02)
                       + 105.0 * exp(-d * d);
            float t = (float)((v - a->wf_low) / (a->wf_high - a->wf_low));
            unsigned char px[4];
            as_cmap(0, t, px);
            row[j * 4 + 0] = px[0];
            row[j * 4 + 1] = px[1];
            row[j * 4 + 2] = px[2];
            row[j * 4 + 3] = 255;
        }
    }
    a->wf.row = 0;
    a->wf.count = WF_ROWS;

    /* bode: 1st order low pass at 2 kHz */
    a->bode.n = 40;
    for (int i = 0; i < a->bode.n; i++) {
        double f = a->bode_f1 *
                   pow(a->bode_f2 / a->bode_f1, (double)i / (a->bode.n - 1));
        a->bode.f[i] = f;
        a->bode.g[i] = -10.0 * log10(1.0 + (f / 2000.0) * (f / 2000.0));
        a->bode.ph[i] = -atan(f / 2000.0) * 180.0 / 3.14159265358979;
    }
    app_set_message(a, "演示数据（截图模式）");

    /* 闭环测试页的演示数据：二阶低通 + 轻微高频滚降 */
    a->lbcfg.points = 24;
    a->lb.fs = fs;
    a->lb.nfreq = 24;
    double lr = log(20000.0 / 20.0);
    for (int i = 0; i < 24; i++) {
        double f = 20.0 * exp(lr * i / 23.0);
        a->lb.freqs[i] = f;
        double g = -10.0 * log10(1.0 + pow(f / 6800.0, 4.0));
        a->lb.fr[i].f = f;
        a->lb.fr[i].gain_db = g;
        a->lb.fr[i].gain_raw_db = g;
        a->lb.fr[i].phase_deg = -atan2(2.0 * (f / 6800.0), 1.0 - pow(f / 6800.0, 2.0)) * 57.29578;
        a->lb.fr[i].amp_err_db = 0;
    }
    a->lb.nfr = 24;
    a->lb.rep.res = a->lb.res;
    a->lb.rep.nfr = 24;
    for (int i = 0; i < 24; i++) a->lb.rep.fr[i] = a->lb.fr[i];
    a->lb.rep.points = 24;
    a->lb.rep.f1 = 20;
    a->lb.rep.f2 = 20000;
    a->lb.rep.fs = fs;
    a->lb.rep.level_dbfs = -12;
    a->lb.rep.tol_db = 0.5;
    a->lb.rep.use_calib = 1;
    an_evaluate(a->lb.fr, a->lb.nfr, NULL, &a->lb.res, a->lb.rep.tol_db);
    a->lb.res.have_delay = a->lb.res.have_step = 1;
    a->lb.res.have_thd = a->lb.res.have_noise = 1;
    a->lb.res.delay_ms = 11.4;
    a->lb.res.rise_us = 41.0;
    a->lb.res.noise_dbfs = -93.2;
    a->lb.res.thd_percent = 0.061;
    a->lb.res.thdn_percent = 0.084;
    a->lb.rep.res = a->lb.res;
    a->lb.rep.nfr = 24;
    for (int i = 0; i < 24; i++) a->lb.rep.fr[i] = a->lb.fr[i];
    a->lb.rep.points = 24;
    a->lb.rep.f1 = 20;
    a->lb.rep.f2 = 20000;
    a->lb.rep.fs = fs;
    a->lb.rep.level_dbfs = -12;
    a->lb.rep.tol_db = 0.5;
    a->lb.rep.use_calib = 1;
    report_build(&a->lb.rep, NULL);
    a->lb.cfg = a->lbcfg;
    a->lb.cfg.points = 24;
    a->lb.fs = fs;
    a->lb.state = LB_DONE;
    a->lb.progress = 1.0;
    a->cal.valid = 1;
    g_reduce_motion = 1;      /* 截图不带过渡动画 */
}

static int render_png(App *a, int window, const char *path, int w, int h) {
    const int scale = 2;              /* 2x 渲染，截图才够锐利 */
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx = CGBitmapContextCreate(
        NULL, (size_t)w * scale, (size_t)h * scale, 8,
        (size_t)w * scale * 4, cs,
        (CGBitmapInfo)kCGImageAlphaPremultipliedLast);
    if (!ctx) {
        CGColorSpaceRelease(cs);
        return -1;
    }
    CGContextTranslateCTM(ctx, 0, h * scale);
    CGContextScaleCTM(ctx, scale, -scale);
    app_draw(a, ctx, w, h, window);
    CGImageRef img = CGBitmapContextCreateImage(ctx);
    CGContextRelease(ctx);
    CGColorSpaceRelease(cs);
    if (!img) return -1;
    CFStringRef p = CFStringCreateWithCString(NULL, path, kCFStringEncodingUTF8);
    CFURLRef url = CFURLCreateWithFileSystemPath(NULL, p, kCFURLPOSIXPathStyle,
                                                 false);
    CFRelease(p);
    CGImageDestinationRef dst = CGImageDestinationCreateWithURL(
        url, CFSTR("public.png"), 1, NULL);
    CFRelease(url);
    int rc = -1;
    if (dst) {
        CGImageDestinationAddImage(dst, img, NULL);
        rc = CGImageDestinationFinalize(dst) ? 0 : -1;
        CFRelease(dst);
    }
    CGImageRelease(img);
    return rc;
}

/* ------------------------------------------------------------------ */
/* view + window                                                       */
/* ------------------------------------------------------------------ */
@interface ScopeView : NSView
@property(nonatomic, assign) int kind;
@end

@implementation ScopeView
- (BOOL)isFlipped { return YES; }
- (BOOL)acceptsFirstResponder { return YES; }

- (UI *)ui {
    return (self.kind == WIN_SOURCE) ? &g_app.ui_src : &g_app.ui;
}

- (void)drawRect:(NSRect)dirty {
    (void)dirty;
    NSGraphicsContext *gc = [NSGraphicsContext currentContext];
    CGContextRef ctx = gc.CGContext;
    NSRect b = self.bounds;
    app_draw(&g_app, ctx, b.size.width, b.size.height, self.kind);
}

- (NSPoint)pointOf:(NSEvent *)e {
    return [self convertPoint:e.locationInWindow fromView:nil];
}

- (void)mouseDown:(NSEvent *)e {
    NSPoint p = [self pointOf:e];
    ui_mouse_down([self ui], p.x, p.y);
    [self setNeedsDisplay:YES];
}

- (void)mouseUp:(NSEvent *)e {
    NSPoint p = [self pointOf:e];
    ui_mouse_up([self ui], p.x, p.y);
    [self setNeedsDisplay:YES];
}

- (void)mouseDragged:(NSEvent *)e {
    NSPoint p = [self pointOf:e];
    ui_mouse_move([self ui], p.x, p.y);
    [self setNeedsDisplay:YES];
}

- (void)scrollWheel:(NSEvent *)e {
    double dy = e.hasPreciseScrollingDeltas ? e.scrollingDeltaY : e.deltaY;
    if (dy == 0.0) return;
    int dir = (dy > 0) ? 1 : -1;
    BOOL ctrl = (e.modifierFlags & NSEventModifierFlagControl) != 0;
    if (self.kind != WIN_MAIN) return;
    if (ctrl) app_zoom_volt(&g_app, dir);
    else app_zoom_time(&g_app, dir);
    [self setNeedsDisplay:YES];
}

- (void)keyDown:(NSEvent *)e {
    NSString *s = e.charactersIgnoringModifiers;
    if (s.length == 0) return;
    unichar c = [s characterAtIndex:0];
    UI *u = [self ui];
    if (c == NSF5FunctionKey) {
        if (g_app.running) app_stop(&g_app);
        else app_start(&g_app);
    } else if (c == NSF6FunctionKey) {
        app_auto_setup(&g_app);
    } else if (c == 27) {
        ui_key(u, KEY_ESC);
    } else if (c == 13 || c == 3) {
        ui_key(u, KEY_ENTER);
    } else if (c == 127 || c == 8) {
        ui_key(u, KEY_BACKSPACE);
    } else if (c == ' ' && self.kind == WIN_MAIN) {
        g_app.frozen = !g_app.frozen;
        app_set_message(&g_app, g_app.frozen ? "已冻结" : "已继续");
    } else if ((c == 't' || c == 'T') && self.kind == WIN_MAIN) {
        app_theme_toggle(&g_app);
    } else if (c == NSLeftArrowFunctionKey && self.kind == WIN_MAIN) {
        app_zoom_time(&g_app, -1);
    } else if (c == NSRightArrowFunctionKey && self.kind == WIN_MAIN) {
        app_zoom_time(&g_app, 1);
    } else if (c >= 32 && c < 127) {
        ui_char(u, (int)c);
    } else {
        [super keyDown:e];
        return;
    }
    [self setNeedsDisplay:YES];
}
@end

static NSWindow *make_window(NSString *title, CGFloat w, CGFloat h, int kind) {
    NSRect frame = NSMakeRect(0, 0, w, h);
    NSWindow *win = [[NSWindow alloc]
        initWithContentRect:frame
                  styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                             NSWindowStyleMaskMiniaturizable |
                             NSWindowStyleMaskResizable)
                    backing:NSBackingStoreBuffered
                      defer:NO];
    win.title = title;
    win.backgroundColor = [NSColor colorWithCalibratedRed:0.09
                                                    green:0.10
                                                     blue:0.10
                                                    alpha:1.0];
    win.minSize = NSMakeSize(1180, 760);
    win.titlebarAppearsTransparent = YES;
    ScopeView *v = [[ScopeView alloc] initWithFrame:frame];
    v.kind = kind;
    v.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    win.contentView = v;
    [win setReleasedWhenClosed:NO];
    return win;
}


/* ------------------------------------------------------------------ */
/* app delegate                                                        */
/* ------------------------------------------------------------------ */
@interface AppDelegate : NSObject <NSApplicationDelegate>
@property(nonatomic, strong) NSWindow *mainWin;
@property(nonatomic, strong) NSWindow *srcWin;
@property(nonatomic, strong) NSTimer *timer;
@end

@implementation AppDelegate

- (void)buildMenu {
    NSMenu *bar = [[NSMenu alloc] init];
    NSMenuItem *appItem = [[NSMenuItem alloc] init];
    [bar addItem:appItem];
    NSMenu *appMenu = [[NSMenu alloc] initWithTitle:@"AudioScope"];
    [appMenu addItemWithTitle:@"关于 AudioScope" action:nil keyEquivalent:@""];
    [appMenu addItem:[NSMenuItem separatorItem]];
    [appMenu addItemWithTitle:@"退出 AudioScope"
                       action:@selector(terminate:)
                keyEquivalent:@"q"];
    appItem.submenu = appMenu;

    NSMenuItem *fileItem = [[NSMenuItem alloc] init];
    [bar addItem:fileItem];
    NSMenu *fileMenu = [[NSMenu alloc] initWithTitle:@"文件"];
    NSMenuItem *mi;
    mi = [fileMenu addItemWithTitle:@"打开音频文件…"
                             action:@selector(openFile:)
                      keyEquivalent:@"o"];
    mi.target = self;
    mi = [fileMenu addItemWithTitle:@"保存 WAV…"
                             action:@selector(saveWav:)
                      keyEquivalent:@"s"];
    mi.target = self;
    mi = [fileMenu addItemWithTitle:@"保存 CSV…"
                             action:@selector(saveCsv:)
                      keyEquivalent:@"S"];
    mi.target = self;
    mi = [fileMenu addItemWithTitle:@"保存频响 CSV…"
                             action:@selector(saveBode:)
                      keyEquivalent:@"b"];
    mi.target = self;
    mi = [fileMenu addItemWithTitle:@"截图…"
                             action:@selector(takeShot:)
                      keyEquivalent:@"p"];
    mi.target = self;
    fileItem.submenu = fileMenu;

    NSMenuItem *runItem = [[NSMenuItem alloc] init];
    [bar addItem:runItem];
    NSMenu *runMenu = [[NSMenu alloc] initWithTitle:@"运行"];
    mi = [runMenu addItemWithTitle:@"开始 / 停止采集"
                            action:@selector(toggleRun:)
                     keyEquivalent:@"r"];
    mi.target = self;
    mi = [runMenu addItemWithTitle:@"AUTO 自适应"
                            action:@selector(autoSetup:)
                     keyEquivalent:@"A"];
    mi.keyEquivalentModifierMask =
        NSEventModifierFlagCommand | NSEventModifierFlagShift;
    mi.target = self;
    mi = [runMenu addItemWithTitle:@"复位缩放"
                            action:@selector(resetView:)
                     keyEquivalent:@"0"];
    mi.target = self;
    [runMenu addItem:[NSMenuItem separatorItem]];
    mi = [runMenu addItemWithTitle:@"信号源窗口"
                            action:@selector(showSource:)
                     keyEquivalent:@"g"];
    mi.target = self;
    [runMenu addItem:[NSMenuItem separatorItem]];
    mi = [runMenu addItemWithTitle:@"闭环测试"
                            action:@selector(showLoopback:)
                     keyEquivalent:@"l"];
    mi.target = self;
    mi = [runMenu addItemWithTitle:@"直连校准（输出→输入）"
                            action:@selector(runCalibration:)
                     keyEquivalent:@""];
    mi.target = self;
    mi = [runMenu addItemWithTitle:@"开始闭环测试"
                            action:@selector(runLoopback:)
                     keyEquivalent:@"L"];
    mi.keyEquivalentModifierMask =
        NSEventModifierFlagCommand | NSEventModifierFlagShift;
    mi.target = self;
    runItem.submenu = runMenu;
    [NSApp setMainMenu:bar];
}

- (void)applicationDidFinishLaunching:(NSNotification *)note {
    (void)note;
    [self buildMenu];
    NSRect screen = NSScreen.mainScreen.visibleFrame;
    CGFloat mw = MIN(1500, screen.size.width * 0.95);
    CGFloat mh = MIN(950, screen.size.height * 0.95);
    self.mainWin = make_window(
        @"AudioScope — 声卡示波器 / 频谱仪 / 信号发生器", mw, mh, WIN_MAIN);
    self.srcWin =
        make_window(@"信号源 — 信号发生器 / 音频文件", 420, 600, WIN_SOURCE);
    [self.mainWin
        setFrameOrigin:NSMakePoint(screen.origin.x + 40,
                                   screen.origin.y + screen.size.height - mh -
                                       40)];
    [self.srcWin setFrameOrigin:NSMakePoint(
                                    screen.origin.x + 60,
                                    screen.origin.y + 40)];
    [self.mainWin makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];

    self.timer = [NSTimer scheduledTimerWithTimeInterval:1.0 / 30.0
                                                  target:self
                                                selector:@selector(tick:)
                                                userInfo:nil
                                                 repeats:YES];
    [[NSRunLoop currentRunLoop] addTimer:self.timer
                                 forMode:NSRunLoopCommonModes];
}

- (void)tick:(NSTimer *)t {
    (void)t;
    app_tick(&g_app);
    if (g_app.want_source) {
        g_app.want_source = 0;
        [self showSource:nil];
    }
    if (g_app.want_open_file) {
        g_app.want_open_file = 0;
        [self openFileAction];
    }
    if (g_app.want_open_dir) {
        g_app.want_open_dir = 0;
        NSString *p = [NSString stringWithUTF8String:g_app.out_dir];
        [[NSWorkspace sharedWorkspace] openURL:[NSURL fileURLWithPath:p]];
    }
    if (self.mainWin.isVisible) [self.mainWin.contentView setNeedsDisplay:YES];
    if (self.srcWin.isVisible) [self.srcWin.contentView setNeedsDisplay:YES];
}

- (void)applicationWillTerminate:(NSNotification *)note {
    (void)note;
    app_save_settings(&g_app);
    engine_stop(&g_app.eng);
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
    (void)sender;
    return YES;
}


- (void)openFileAction {
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    panel.canChooseFiles = YES;
    panel.canChooseDirectories = NO;
    panel.allowsMultipleSelection = NO;
    panel.message = @"选择要播放的音频文件";
    if (g_app.last_dir[0]) {
        NSString *dir = [NSString stringWithUTF8String:g_app.last_dir];
        panel.directoryURL = [NSURL fileURLWithPath:dir];
    }
    if ([panel runModal] == NSModalResponseOK) {
        NSURL *url = panel.URL;
        if (url) app_load_file(&g_app, url.fileSystemRepresentation);
    }
}

- (NSString *)savePath:(NSString *)name ext:(NSString *)ext {
    NSSavePanel *panel = [NSSavePanel savePanel];
    panel.nameFieldStringValue = name;
    if (ext) {
        UTType *t = [UTType typeWithFilenameExtension:ext];
        if (t) panel.allowedContentTypes = @[ t ];
    }
    if ([panel runModal] != NSModalResponseOK) return nil;
    return panel.URL.path;
}

- (IBAction)openFile:(id)sender {
    (void)sender;
    [self openFileAction];
}

- (IBAction)toggleRun:(id)sender {
    (void)sender;
    if (g_app.running) app_stop(&g_app);
    else app_start(&g_app);
}

- (IBAction)autoSetup:(id)sender {
    (void)sender;
    app_auto_setup(&g_app);
}

- (IBAction)resetView:(id)sender {
    (void)sender;
    app_reset_view(&g_app);
}

- (IBAction)showSource:(id)sender {
    (void)sender;
    [self.srcWin makeKeyAndOrderFront:nil];
    [self.srcWin orderFrontRegardless];
}

- (IBAction)showLoopback:(id)sender {
    (void)sender;
    g_app.tab = TAB_LOOP;
    [self.mainWin makeKeyAndOrderFront:nil];
}

- (IBAction)runCalibration:(id)sender {
    (void)sender;
    g_app.tab = TAB_LOOP;
    app_lb_start(&g_app, 1);
}

- (IBAction)runLoopback:(id)sender {
    (void)sender;
    g_app.tab = TAB_LOOP;
    app_lb_start(&g_app, 0);
}

- (IBAction)saveWav:(id)sender {
    (void)sender;
    NSString *p = [self savePath:@"audioscope.wav" ext:@"wav"];
    if (p) app_export_wav(&g_app, p.fileSystemRepresentation);
}

- (IBAction)saveCsv:(id)sender {
    (void)sender;
    NSString *p = [self savePath:@"audioscope.csv" ext:@"csv"];
    if (p) app_export_csv(&g_app, p.fileSystemRepresentation);
}

- (IBAction)saveBode:(id)sender {
    (void)sender;
    NSString *p = [self savePath:@"bode.csv" ext:@"csv"];
    if (p) app_export_bode(&g_app, p.fileSystemRepresentation);
}

- (IBAction)takeShot:(id)sender {
    (void)sender;
    NSString *p = [self savePath:@"audioscope.png" ext:@"png"];
    if (!p) return;
    NSWindow *w = NSApp.keyWindow ? NSApp.keyWindow : self.mainWin;
    NSView *v = w.contentView;
    NSBitmapImageRep *rep = [v bitmapImageRepForCachingDisplayInRect:v.bounds];
    [v cacheDisplayInRect:v.bounds toBitmapImageRep:rep];
    NSData *png = [rep representationUsingType:NSBitmapImageFileTypePNG
                                    properties:@{}];
    [png writeToFile:p atomically:YES];
    app_set_message(&g_app, "截图已保存: %s", p.fileSystemRepresentation);
}
@end



/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */
static int shot_mode(const char *dir) {
    char path[1024];
    mkdir(dir, 0755);
    app_init(&g_app);
    demo_fill(&g_app);
    g_app.tab = TAB_SCOPE;
    snprintf(path, sizeof(path), "%s/0_scope.png", dir);
    render_png(&g_app, WIN_MAIN, path, 1440, 900);
    g_app.tab = TAB_FFT;
    snprintf(path, sizeof(path), "%s/1_fft.png", dir);
    render_png(&g_app, WIN_MAIN, path, 1440, 900);
    g_app.tab = TAB_BODE;
    snprintf(path, sizeof(path), "%s/2_bode.png", dir);
    render_png(&g_app, WIN_MAIN, path, 1440, 900);
    g_app.tab = TAB_LOOP;
    snprintf(path, sizeof(path), "%s/3_loop.png", dir);
    render_png(&g_app, WIN_MAIN, path, 1440, 900);
    theme_set(THEME_LIGHT);
    snprintf(path, sizeof(path), "%s/4_light.png", dir);
    render_png(&g_app, WIN_MAIN, path, 1440, 900);
    theme_set(THEME_DARK);
    g_app.tab = TAB_SCOPE;
    snprintf(path, sizeof(path), "%s/5_source.png", dir);
    render_png(&g_app, WIN_SOURCE, path, 460, 720);
    printf("screenshots written to %s\n", dir);
    return 0;
}

static int list_devices_cmd(void) {
    ASDevice devs[48];
    int n = as_devices(devs, 48);
    printf("%-6s %-44s %4s %4s %8s\n", "ID", "NAME", "IN", "OUT", "RATE");
    for (int i = 0; i < n; i++)
        printf("%-6u %-44s %4d %4d %8.0f\n", (unsigned)devs[i].id,
               devs[i].name, devs[i].in_ch, devs[i].out_ch, devs[i].rate);
    return 0;
}

static int audio_test(void) {
    app_init(&g_app);
    printf("Devices:\n");
    list_devices_cmd();
    g_app.gen.wave = W_SINE;
    g_app.gen.freq = 440.0;
    g_app.gen.amp_db = -46.0;
    g_app.gen.enabled = 1;
    g_app.gen.ch_en[0] = 1;
    app_start(&g_app);
    if (!g_app.running) {
        printf("engine start failed: %s\n", g_app.msg);
        return 1;
    }
    printf("started: fs=%.0f in_rate=%.0f msg=%s\n", g_app.fs, g_app.eng.in_rate,
           g_app.msg);
    for (int i = 0; i < 60; i++) {
        app_tick(&g_app);
        usleep(30000);
    }
    printf("in_cb=%d out_cb=%d xrun=%d in_peak=%.1f dBFS\n",
           g_app.eng.in_cb_count, g_app.eng.out_cb_count, g_app.eng.xruns,
           as_db(g_app.eng.in_peak[0]));
    if (g_app.running) {
        int64_t got = engine_read(&g_app.eng, g_app.cap, 4096);
        double peak = 0;
        for (int64_t i = 0; i < got * 2; i++)
            if (fabs(g_app.cap[i]) > peak) peak = fabs(g_app.cap[i]);
        printf("captured %lld frames, peak %.1f dBFS, scope n=%d\n",
               (long long)got, as_db(peak), g_app.scope.n);
    }
    app_stop(&g_app);
    printf("AUDIO TEST DONE\n");
    return 0;
}

/* CLI 模式：刷新后直接退出，跳过 AppKit 运行时析构阶段（GUI 模式不走这里）。 */
static int finish_cli(int rc) {
    fflush(stdout);
    fflush(stderr);
    _exit(rc);
}

int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--selftest")) return finish_cli(run_selftest());
        if (!strcmp(argv[i], "--devices")) return finish_cli(list_devices_cmd());
        if (!strcmp(argv[i], "--audiotest")) return finish_cli(audio_test());
        if (!strcmp(argv[i], "--unittest")) return finish_cli(run_unittest());
        if (!strcmp(argv[i], "--simulate"))
            return finish_cli(
                run_simulate((i + 1 < argc) ? argv[i + 1] : "build/sim"));
        if (!strcmp(argv[i], "--shot"))
            return finish_cli(shot_mode((i + 1 < argc) ? argv[i + 1] : "."));
        if (!strcmp(argv[i], "--help")) {
            printf("AudioScope C 1.1\n"
                   "  --selftest        run DSP self tests\n"
                   "  --unittest        run waveform/analysis/report unit tests\n"
                   "  --simulate <dir>  run the closed-loop analysis on a "
                   "known virtual DUT\n"
                   "  --devices         list audio devices\n"
                   "  --audiotest       open devices, run 2 s, report levels\n"
                   "  --shot <dir>      render UI screenshots\n");
            return 0;
        }
    }
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        [app setActivationPolicy:NSApplicationActivationPolicyRegular];
        AppDelegate *del = [[AppDelegate alloc] init];
        app.delegate = del;
        app_init(&g_app);
        [app run];
    }
    return 0;
}
