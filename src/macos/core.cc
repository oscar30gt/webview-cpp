#ifdef __APPLE__

#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>
#include <string>
#include <sstream>

#include "../webviewWindow.h"

// -------- Window Delegate -----------------------------------------------------------------------

@interface _WebviewWindowDelegate : NSObject <NSWindowDelegate>
@property (nonatomic, assign) WebviewWindow* cppWindow;
@end

@implementation _WebviewWindowDelegate

- (void)windowDidResize:(NSNotification*)notification {
    if (!_cppWindow) return;
    NSWindow* win = (NSWindow*)notification.object;
    NSRect rect = [win contentRectForFrameRect:win.frame];
    Json::Value sizeChangeDetail;
    sizeChangeDetail["width"] = (int)rect.size.width;
    sizeChangeDetail["height"] = (int)rect.size.height;
    _cppWindow->emit("webview:resized", sizeChangeDetail);
}

- (void)windowDidMove:(NSNotification*)notification {
    if (!_cppWindow) return;
    NSWindow* win = (NSWindow*)notification.object;
    CGFloat screenHeight = 0;
    if (NSScreen.screens.count > 0) {
        screenHeight = NSScreen.screens[0].frame.size.height;
    }
    NSRect frame = win.frame;
    int x = (int)frame.origin.x;
    int y = (int)(screenHeight - frame.origin.y - frame.size.height);
    Json::Value moveDetail;
    moveDetail["x"] = x;
    moveDetail["y"] = y;
    _cppWindow->emit("webview:moved", moveDetail);
}

- (void)windowDidMiniaturize:(NSNotification*)notification {
    if (_cppWindow) _cppWindow->emit("webview:minimized");
}

- (void)windowDidDeminiaturize:(NSNotification*)notification {
    if (_cppWindow) _cppWindow->emit("webview:restored");
}

- (void)windowDidEnterFullScreen:(NSNotification*)notification {
    if (_cppWindow) _cppWindow->emit("webview:maximized");
}

- (void)windowDidExitFullScreen:(NSNotification*)notification {
    if (_cppWindow) _cppWindow->emit("webview:restored");
}

- (BOOL)windowShouldClose:(NSWindow*)sender {
    if (_cppWindow) {
        _cppWindow->emit("webview:close-requested");
    }
    return YES;
}

@end

// -------- Window Creation & Destruction ---------------------------------------------------------

NativeWindowHandle createWebviewWindow(int width, int height) {
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

    NSRect contentRect = NSMakeRect(100, 100, (CGFloat)width, (CGFloat)height);
    NSWindowStyleMask styleMask =
        NSWindowStyleMaskTitled |
        NSWindowStyleMaskClosable |
        NSWindowStyleMaskMiniaturizable |
        NSWindowStyleMaskResizable;

    NSWindow* win = [[NSWindow alloc]
        initWithContentRect:contentRect
        styleMask:styleMask
        backing:NSBackingStoreBuffered
        defer:NO];

    [win setTitle:@"WebviewWindow"];

    WKWebViewConfiguration* config = [[WKWebViewConfiguration alloc] init];
    WKWebView* wv = [[WKWebView alloc] initWithFrame:[win.contentView bounds] configuration:config];
    [wv setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [win.contentView addSubview:wv];

    [win center];
    [win makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];

    NativeWindowHandle handle;
    handle.nsWindow = (__bridge_retained void*)win;
    handle.wkWebView = (__bridge_retained void*)wv;
    handle.windowDelegate = nullptr;
    handle.scriptDelegate = nullptr;
    handle.ready = true;
    return handle;
}

void destroyWebviewWindow(NativeWindowHandle& handle) {
    if (handle.windowDelegate) {
        _WebviewWindowDelegate* delegate = (__bridge_transfer _WebviewWindowDelegate*)handle.windowDelegate;
        delegate = nil;
        handle.windowDelegate = nullptr;
    }
    if (handle.scriptDelegate) {
        id scriptHandler = (__bridge_transfer id)handle.scriptDelegate;
        scriptHandler = nil;
        handle.scriptDelegate = nullptr;
    }
    if (handle.nsWindow) {
        NSWindow* win = (__bridge_transfer NSWindow*)handle.nsWindow;
        [win setDelegate:nil];
        [win close];
        handle.nsWindow = nullptr;
    }
    if (handle.wkWebView) {
        WKWebView* wv = (__bridge_transfer WKWebView*)handle.wkWebView;
        wv = nil;
        handle.wkWebView = nullptr;
    }
}

// -------- Event Subscription --------------------------------------------------------------------

void WebviewWindow::subscribeToWindowEvents() {
    NSWindow* win = (__bridge NSWindow*)m_windowHandle.nsWindow;
    if (!win) return;

    _WebviewWindowDelegate* delegate = [[_WebviewWindowDelegate alloc] init];
    delegate.cppWindow = this;
    [win setDelegate:delegate];
    m_windowHandle.windowDelegate = (__bridge_retained void*)delegate;
}

// -------- Window Lifecycle ----------------------------------------------------------------------

void WebviewWindow::run() {
    [NSApp run];
}

void WebviewWindow::close() {
    NSWindow* win = (__bridge NSWindow*)m_windowHandle.nsWindow;
    if (win) {
        [win performClose:nil];
    }
}

void WebviewWindow::navigate(const std::string& url) {
    WKWebView* wv = (__bridge WKWebView*)m_windowHandle.wkWebView;
    if (!wv) return;

    NSString* nsUrlStr = [NSString stringWithUTF8String:url.c_str()];
    NSURL* nsUrl = [NSURL URLWithString:nsUrlStr];
    if (!nsUrl || !nsUrl.scheme) {
        nsUrl = [NSURL fileURLWithPath:nsUrlStr];
    }
    NSURLRequest* req = [NSURLRequest requestWithURL:nsUrl];
    [wv loadRequest:req];
}

void WebviewWindow::eval(const std::string& jsCode) {
    WKWebView* wv = (__bridge WKWebView*)m_windowHandle.wkWebView;
    if (!wv) return;

    NSString* script = [NSString stringWithUTF8String:jsCode.c_str()];
    [wv evaluateJavaScript:script completionHandler:nil];
}

void WebviewWindow::evalOnDocumentCreated(const std::string& jsCode) {
    WKWebView* wv = (__bridge WKWebView*)m_windowHandle.wkWebView;
    if (!wv) return;

    NSString* script = [NSString stringWithUTF8String:jsCode.c_str()];
    WKUserScript* userScript = [[WKUserScript alloc]
        initWithSource:script
        injectionTime:WKUserScriptInjectionTimeAtDocumentStart
        forMainFrameOnly:NO];
    [wv.configuration.userContentController addUserScript:userScript];
}

// -------- Window Sizing & Constraints ------------------------------------------------------------

void WebviewWindow::setSize(int width, int height) {
    NSWindow* win = (__bridge NSWindow*)m_windowHandle.nsWindow;
    if (!win) return;

    NSRect frame = win.frame;
    CGFloat oldHeight = frame.size.height;
    frame.size.width = (CGFloat)width;
    frame.size.height = (CGFloat)height;
    frame.origin.y -= (frame.size.height - oldHeight); // Preserve top-left position
    [win setFrame:frame display:YES animate:NO];
}

void WebviewWindow::setMinSize(int minWidth, int minHeight) {
    NSWindow* win = (__bridge NSWindow*)m_windowHandle.nsWindow;
    if (!win) return;
    [win setMinSize:NSMakeSize((CGFloat)minWidth, (CGFloat)minHeight)];
}

void WebviewWindow::setMaxSize(int maxWidth, int maxHeight) {
    NSWindow* win = (__bridge NSWindow*)m_windowHandle.nsWindow;
    if (!win) return;
    if (maxWidth > 0 && maxHeight > 0) {
        [win setMaxSize:NSMakeSize((CGFloat)maxWidth, (CGFloat)maxHeight)];
    } else {
        [win setMaxSize:NSMakeSize(FLT_MAX, FLT_MAX)];
    }
}

void WebviewWindow::setResizable(bool resizable) {
    NSWindow* win = (__bridge NSWindow*)m_windowHandle.nsWindow;
    if (!win) return;
    if (resizable) {
        win.styleMask |= NSWindowStyleMaskResizable;
    } else {
        win.styleMask &= ~NSWindowStyleMaskResizable;
    }
}

bool WebviewWindow::isResizable() const {
    NSWindow* win = (__bridge NSWindow*)m_windowHandle.nsWindow;
    if (!win) return false;
    return (win.styleMask & NSWindowStyleMaskResizable) != 0;
}

// -------- Window Manipulation -------------------------------------------------------------------

void WebviewWindow::setTitle(const std::string& title) {
    NSWindow* win = (__bridge NSWindow*)m_windowHandle.nsWindow;
    if (!win) return;
    [win setTitle:[NSString stringWithUTF8String:title.c_str()]];
}

void WebviewWindow::setAlwaysOnTop(bool alwaysOnTop) {
    NSWindow* win = (__bridge NSWindow*)m_windowHandle.nsWindow;
    if (!win) return;
    [win setLevel:alwaysOnTop ? NSFloatingWindowLevel : NSNormalWindowLevel];
}

void WebviewWindow::setPosition(int x, int y) {
    NSWindow* win = (__bridge NSWindow*)m_windowHandle.nsWindow;
    if (!win) return;
    CGFloat screenHeight = (NSScreen.screens.count > 0) ? NSScreen.screens[0].frame.size.height : 0;
    NSRect frame = win.frame;
    frame.origin.x = (CGFloat)x;
    frame.origin.y = screenHeight - (CGFloat)y - frame.size.height;
    [win setFrame:frame display:YES];
}

void WebviewWindow::move(int deltaX, int deltaY) {
    NSWindow* win = (__bridge NSWindow*)m_windowHandle.nsWindow;
    if (!win) return;
    NSRect frame = win.frame;
    frame.origin.x += (CGFloat)deltaX;
    frame.origin.y -= (CGFloat)deltaY; // Invert Y delta for Cocoa coordinate system
    [win setFrame:frame display:YES];
}

void WebviewWindow::maximize() {
    NSWindow* win = (__bridge NSWindow*)m_windowHandle.nsWindow;
    if (!win) return;
    if (!win.isZoomed) {
        [win zoom:nil];
    }
}

void WebviewWindow::minimize() {
    NSWindow* win = (__bridge NSWindow*)m_windowHandle.nsWindow;
    if (!win) return;
    [win miniaturize:nil];
}

void WebviewWindow::restore() {
    NSWindow* win = (__bridge NSWindow*)m_windowHandle.nsWindow;
    if (!win) return;
    if (win.isMiniaturized) {
        [win deminiaturize:nil];
    } else if (win.isZoomed) {
        [win zoom:nil];
    }
}

void WebviewWindow::show() {
    NSWindow* win = (__bridge NSWindow*)m_windowHandle.nsWindow;
    if (!win) return;
    [win makeKeyAndOrderFront:nil];
}

void WebviewWindow::hide() {
    NSWindow* win = (__bridge NSWindow*)m_windowHandle.nsWindow;
    if (!win) return;
    [win orderOut:nil];
}

WindowSize WebviewWindow::getSize() const {
    NSWindow* win = (__bridge NSWindow*)m_windowHandle.nsWindow;
    if (!win) return { 0, 0 };
    NSRect frame = win.frame;
    return { (int)frame.size.width, (int)frame.size.height };
}

WindowPosition WebviewWindow::getPosition() const {
    NSWindow* win = (__bridge NSWindow*)m_windowHandle.nsWindow;
    if (!win) return { 0, 0 };
    CGFloat screenHeight = (NSScreen.screens.count > 0) ? NSScreen.screens[0].frame.size.height : 0;
    NSRect frame = win.frame;
    int x = (int)frame.origin.x;
    int y = (int)(screenHeight - frame.origin.y - frame.size.height);
    return { x, y };
}

bool WebviewWindow::isAlwaysOnTop() const {
    NSWindow* win = (__bridge NSWindow*)m_windowHandle.nsWindow;
    if (!win) return false;
    return win.level == NSFloatingWindowLevel;
}

bool WebviewWindow::isMaximized() const {
    NSWindow* win = (__bridge NSWindow*)m_windowHandle.nsWindow;
    if (!win) return false;
    return win.isZoomed;
}

bool WebviewWindow::isMinimized() const {
    NSWindow* win = (__bridge NSWindow*)m_windowHandle.nsWindow;
    if (!win) return false;
    return win.isMiniaturized;
}

bool WebviewWindow::isRestored() const {
    NSWindow* win = (__bridge NSWindow*)m_windowHandle.nsWindow;
    if (!win) return false;
    return !win.isZoomed && !win.isMiniaturized;
}

bool WebviewWindow::isHidden() const {
    NSWindow* win = (__bridge NSWindow*)m_windowHandle.nsWindow;
    if (!win) return false;
    return !win.isVisible;
}

// -------- Style ---------------------------------------------------------------------------------

void WebviewWindow::setBackdrop(WindowBackdropEffect backdrop) {
    NSWindow* win = (__bridge NSWindow*)m_windowHandle.nsWindow;
    WKWebView* wv = (__bridge WKWebView*)m_windowHandle.wkWebView;
    if (!win || !wv) return;

    // Remove any existing visual effect view
    for (NSView* subview in [win.contentView.subviews copy]) {
        if ([subview isKindOfClass:[NSVisualEffectView class]]) {
            [subview removeFromSuperview];
        }
    }

    switch (backdrop) {
        case WindowBackdropEffect::Solid: {
            [win setOpaque:YES];
            [win setBackgroundColor:[NSColor windowBackgroundColor]];
            [wv setValue:@YES forKey:@"drawsBackground"];
            break;
        }
        case WindowBackdropEffect::Vibrant: {
            [win setOpaque:NO];
            [win setBackgroundColor:[NSColor clearColor]];
            [wv setValue:@NO forKey:@"drawsBackground"];

            NSVisualEffectView* effectView = [[NSVisualEffectView alloc] initWithFrame:win.contentView.bounds];
            [effectView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
            [effectView setBlendingMode:NSVisualEffectBlendingModeBehindWindow];
            [effectView setMaterial:NSVisualEffectMaterialUnderWindowBackground];
            [effectView setState:NSVisualEffectStateActive];

            [win.contentView addSubview:effectView positioned:NSWindowBelow relativeTo:wv];
            break;
        }
        case WindowBackdropEffect::Transparent: {
            [win setOpaque:NO];
            [win setBackgroundColor:[NSColor clearColor]];
            [wv setValue:@NO forKey:@"drawsBackground"];
            break;
        }
    }
}

void WebviewWindow::setFrame(WindowFrameStyle frameStyle) {
    NSWindow* win = (__bridge NSWindow*)m_windowHandle.nsWindow;
    if (!win) return;

    switch (frameStyle) {
        case WindowFrameStyle::All: {
            win.styleMask |= (NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable);
            win.styleMask &= ~NSWindowStyleMaskFullSizeContentView;
            win.titlebarAppearsTransparent = NO;
            win.titleVisibility = NSWindowTitleVisible;
            break;
        }
        case WindowFrameStyle::NoTitlebar: {
            win.styleMask |= (NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable | NSWindowStyleMaskFullSizeContentView);
            win.titlebarAppearsTransparent = YES;
            win.titleVisibility = NSWindowTitleHidden;
            break;
        }
        case WindowFrameStyle::None: {
            win.styleMask = NSWindowStyleMaskBorderless | NSWindowStyleMaskResizable;
            break;
        }
    }
}

#endif // __APPLE__