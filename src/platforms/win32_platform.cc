/// win32_platform.cc
/// Windows-specific implementation for the WebviewWindow class.

/// Styling Windows's windows is a bit tricky as the Windows API does not
/// provide a clear way to customize the window's appearance without messing other
/// things up. The following implementation uses a combination of Windows API calls
/// and custom window procedures to achieve the desired behavior (or at least, 
/// as close as possible).

#ifdef _WIN32

#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>

#include "../webviewWindow.h"

#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38 // Backdrop type attribute for Windows
#endif

#ifndef DWMSBT_NONE
#define DWMSBT_NONE 1 // No backdrop
#endif

#ifndef DWMSBT_TRANSIENTWINDOW
#define DWMSBT_TRANSIENTWINDOW 3 // Acrylic
#endif

#define WINDOWSTATE_UNKNOWN   0
#define WINDOWSTATE_MAXIMIZED 1
#define WINDOWSTATE_MINIMIZED 2
#define WINDOWSTATE_RESTORED  3

// -------- Helper functions ----------------------------------------------------------------------

/// Gets the native window handle (HWND) from the webview instance.
/// @param wv The webview instance from which to retrieve the window handle.
/// @returns The HWND of the webview window, or nullptr if not available.
HWND getHandleFromWebview(const webview::webview& wv)
{
    auto raw_hwnd = wv.window();
    if (!raw_hwnd.has_value())
    {
        return nullptr;
    }
    return static_cast<HWND>(raw_hwnd.value());
}

// -------- Top resizer window for frameless windows ----------------------------------------------

#define RESIZER_OVERLAY_ID 50
#define RESIZER_HEIGHT     5

/// Retrieves the top resizer overlay child window associated with the given parent window.
/// @param parentHwnd The parent window handle to search within.
/// @returns The HWND of the resizer overlay, or nullptr if not found or parentHwnd is null.
HWND getTopResizerOverlay(HWND parentHwnd)
{
    if (!parentHwnd) return nullptr;
    return GetDlgItem(parentHwnd, RESIZER_OVERLAY_ID);
}

/// Window procedure for the top resizer overlay.
/// Handles hit-testing and forwarding resize drag messages to the parent window.
/// Passes through as transparent when the parent is maximized.
LRESULT CALLBACK ResizerOverlayProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_NCHITTEST:
        {
            // Transparent if maximized or not resizable
            HWND hParent = GetParent(hwnd);
            LONG_PTR win_style = GetWindowLongPtr(hParent, GWL_STYLE);
            bool hasThickframe = (win_style & WS_THICKFRAME) != 0;

            return !hParent || IsZoomed(hParent) || !hasThickframe
                ? TRANSPARENT
                : HTTOP;
        }

        case WM_NCLBUTTONDOWN:
        {
            // Transparent if maximized
            HWND hParent = GetParent(hwnd);
            LONG_PTR win_style = GetWindowLongPtr(hParent, GWL_STYLE);
            bool hasThickframe = (win_style & WS_THICKFRAME) != 0;
            if (!hParent || IsZoomed(hParent) || !hasThickframe) return TRANSPARENT;

            if (wParam == HTTOP)
            {
                if (hParent)
                {
                    SendMessage(hParent, WM_NCLBUTTONDOWN, HTTOP, lParam);
                    return 0;
                }
            }
            break;
        }

        case WM_ERASEBKGND:
            return 1;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

/// Creates and attaches a transparent child window at the top edge of the given parent window.
/// This overlay is used to enable top-edge resizing for frameless windows.
/// Does nothing if the overlay already exists or if parentHwnd is null.
/// @param parentHwnd The parent window to which the resizer overlay is attached.
void createTopResizerOverlay(HWND parentHwnd)
{
    if (!parentHwnd || getTopResizerOverlay(parentHwnd)) return;

    WNDCLASSW wc = { };
    wc.lpfnWndProc = ResizerOverlayProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"ResizerOverlayObject";

    RegisterClassW(&wc);

    RECT parentRect;
    GetClientRect(parentHwnd, &parentRect);
    int width = parentRect.right - parentRect.left;
    if (width <= 0) width = 0;

    HWND hResizer = CreateWindowExW(
        0,
        L"ResizerOverlayObject",
        NULL,
        WS_CHILDWINDOW | WS_VISIBLE,
        0, 0, width, RESIZER_HEIGHT,
        parentHwnd,
        reinterpret_cast<HMENU>(RESIZER_OVERLAY_ID),
        GetModuleHandle(NULL),
        NULL
    );

    if (hResizer)
    {
        BringWindowToTop(hResizer);
        SetWindowPos(hResizer, HWND_TOP, 0, 0, width, RESIZER_HEIGHT, SWP_SHOWWINDOW);
    }
}

/// Destroys the top resizer overlay associated with the given parent window, if it exists.
/// @param parentHwnd The parent window whose resizer overlay is to be removed.
void removeTopResizerOverlay(HWND parentHwnd)
{
    HWND hResizer = getTopResizerOverlay(parentHwnd);
    if (hResizer)
    {
        DestroyWindow(hResizer);
    }
}

// -------- Subclass procedures -------------------------------------------------------------------

/// Subclass procedure for style-related adjustements.
LRESULT CALLBACK StyleSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR /* dwRefData */)
{
    switch (msg)
    {

        // Calc size for non-client area to adjust the window's client area, taking the titlebar space to make it disappear
        case WM_NCCALCSIZE:
        {
            NCCALCSIZE_PARAMS* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
            const int originalTop = params->rgrc[0].top;

            LRESULT ret = DefSubclassProc(hwnd, msg, wParam, lParam);
            if (ret != 0) return ret;

            // When maximized, adjust the client area to fit the whole monitor work area.
            if (IsZoomed(hwnd))
            {
                HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
                MONITORINFO monitorInfo = { };
                if (GetMonitorInfo(monitor, &monitorInfo))
                {
                    params->rgrc[0] = monitorInfo.rcWork;
                }
            }

            // Otherwise, keep the original top position to avoid cutting off the title bar area.
            else
            {
                params->rgrc[0].top = originalTop;
            }

            return 0;
        }

        case WM_SIZE:
        {
            int width = LOWORD(lParam);

            HWND hResizer = getTopResizerOverlay(hwnd);
            if (hResizer)
            {
                SetWindowPos(hResizer, HWND_TOP, 0, 0, width, RESIZER_HEIGHT, SWP_SHOWWINDOW);
                BringWindowToTop(hResizer);
            }

            break;
        }

        case WM_NCDESTROY:
            RemoveWindowSubclass(hwnd, StyleSubclassProc, uIdSubclass);
            break;
    }

    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

/// Subclass procedure for window events (resize, move, close) to notify the WebviewWindow instance.
LRESULT CALLBACK EventSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    // Tracks the current window state to avoid duplicate event emissions.
    static char windowState = WINDOWSTATE_UNKNOWN;

    auto* win = reinterpret_cast<WebviewWindow*>(dwRefData);
    if (!win) return DefSubclassProc(hwnd, uMsg, wParam, lParam);

    switch (uMsg)
    {
        case WM_SIZE:
        {
            // Window states: maximized, minimized, restored
            if (wParam == SIZE_MAXIMIZED && windowState != WINDOWSTATE_MAXIMIZED)
            {
                windowState = WINDOWSTATE_MAXIMIZED;
                win->emit("webview:maximized");
            }
            else if (wParam == SIZE_MINIMIZED && windowState != WINDOWSTATE_MINIMIZED)
            {
                windowState = WINDOWSTATE_MINIMIZED;
                win->emit("webview:minimized");
            }
            else if (wParam == SIZE_RESTORED && windowState != WINDOWSTATE_RESTORED)
            {
                windowState = WINDOWSTATE_RESTORED;
                win->emit("webview:restored");
            }

            // Size change in real-time (width and height)
            // LOWORD(lParam) = Width, HIWORD(lParam) = Height in pixels of the client area
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);

            win->emit("webview:resized", { width, height });
            break;
        }

        case WM_MOVE:
        {
            // To capture negative coordinates on secondary monitors,
            // the macros from <windowsx.h> are used over direct LOWORD/HIWORD.
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);

            win->emit("webview:moved", { x, y });
            break;
        }

        case WM_CLOSE:
        {
            win->emit("webview:close-requested");
            break;
        }

        case WM_NCDESTROY:
        {
            RemoveWindowSubclass(hwnd, EventSubclassProc, uIdSubclass);
            break;
        }
    }

    return DefSubclassProc(hwnd, uMsg, wParam, lParam);
}

// -------- Windows-specific interface implementations --------------------------------------------

void setupPlatformSpecificWindowListeners(WebviewWindow* window)
{
    auto hwnd = getHandleFromWebview(window->m_webview);
    if (!hwnd) return;

    // Subclass the window to intercept messages for resize, move, and close events.
    SetWindowSubclass(hwnd, EventSubclassProc, 2, reinterpret_cast<DWORD_PTR>(window));
}

void setupPlatformSpecificBindings(WebviewWindow* window)
{
    auto hwnd = getHandleFromWebview(window->m_webview);
    if (!hwnd) return;

    window->m_webview.bind(
        "webview:startWindowDrag",
        [hwnd](const std::string&) -> std::string
        {
            ReleaseCapture();
            SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            return "";
        }
    );
}

void setWebviewWindowStyle(webview::webview& wv, WindowStyle style)
{
    auto hwnd = getHandleFromWebview(wv);
    LONG_PTR win_style = GetWindowLongPtr(hwnd, GWL_STYLE);

    const bool isNative = style == WindowStyle::Native;
    const bool isFrameless = style == WindowStyle::Frameless;
    const bool isFramelessVibrant = style == WindowStyle::FramelessVibrant;
    const bool isTransparent = style == WindowStyle::Transparent;
    const bool needsCustomFrame = isFrameless || isFramelessVibrant || isTransparent;
    const bool needsAcrylic = isFramelessVibrant;
    const bool needsTransparency = isFramelessVibrant || isTransparent;

    // ---- Window style ----

    if (isNative)
    {
        win_style |= WS_THICKFRAME | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
    }
    else if (isFrameless || isFramelessVibrant)
    {
        win_style |= WS_THICKFRAME | WS_CAPTION | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
        win_style &= ~WS_SYSMENU;
    }
    else // isTransparent
    {
        win_style &= ~(WS_THICKFRAME | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
    }

    SetWindowLongPtr(hwnd, GWL_STYLE, win_style);

    // ---- DWM frame and backdrop ----

    if (needsCustomFrame)
    {
        MARGINS margins = { -1, -1, -1, -1 };
        DwmExtendFrameIntoClientArea(hwnd, &margins);

        DWORD backdrop = needsAcrylic ? DWMSBT_TRANSIENTWINDOW : DWMSBT_NONE;
        DwmSetWindowAttribute(hwnd, static_cast<DWMWINDOWATTRIBUTE>(DWMWA_SYSTEMBACKDROP_TYPE), &backdrop, sizeof(backdrop));
    }

    // ---- Resizer overlay ----

    if (isFrameless || isFramelessVibrant)
        createTopResizerOverlay(hwnd);
    else
        removeTopResizerOverlay(hwnd);

    // ---- Background brush ----

    HBRUSH bgBrush = needsTransparency
        ? static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH))
        : static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));

    SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND, reinterpret_cast<LONG_PTR>(bgBrush));

    // ---- WebView background color ----

    ICoreWebView2Controller* controllerPtr = nullptr;
    auto raw_widget = wv.browser_controller();

    if (raw_widget.has_value())
    {
        controllerPtr = static_cast<ICoreWebView2Controller*>(raw_widget.value());

        if (needsTransparency)
        {
            ICoreWebView2Controller2* controller2 = nullptr;
            HRESULT hrQI = controllerPtr->QueryInterface(IID_ICoreWebView2Controller2, reinterpret_cast<void**>(&controller2));

            if (SUCCEEDED(hrQI) && controller2)
            {
                COREWEBVIEW2_COLOR transparentColor = { 0, 0, 0, 0 };
                controller2->put_DefaultBackgroundColor(transparentColor);
                controller2->Release();
            }
        }

        RECT bounds;
        GetClientRect(hwnd, &bounds);
        controllerPtr->put_Bounds(bounds);
    }

    // ---- Subclassing for custom-frame windows ----

    if (needsCustomFrame)
        SetWindowSubclass(hwnd, StyleSubclassProc, 1, reinterpret_cast<DWORD_PTR>(controllerPtr));
    else
        RemoveWindowSubclass(hwnd, StyleSubclassProc, 1);

    // Apply style and effects immediately by forcing a redraw of the window.
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    RedrawWindow(hwnd, nullptr, nullptr, RDW_UPDATENOW | RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN);
}

/// Holds minimum and maximum size constraints for a window.
/// Zero values indicate that the corresponding constraint is not enforced.
struct SizeConstraints { int minW = 0, minH = 0, maxW = 0, maxH = 0; };

/// Maps each constrained window handle to its associated SizeConstraints.
/// Populated by setWebviewWindowMinSize / setWebviewWindowMaxSize,
/// and cleaned up automatically on WM_NCDESTROY.
static std::unordered_map<HWND, SizeConstraints> s_sizeConstraints;

/// Subclass procedure that enforces minimum and maximum window size constraints.
/// Reads the SizeConstraints stored in dwRefData and applies them to WM_GETMINMAXINFO.
/// Removes itself and clears the constraints map entry on WM_NCDESTROY.
static LRESULT CALLBACK SizeConstraintsSubclassProc(
    HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    if (msg == WM_GETMINMAXINFO)
    {
        auto* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
        auto* c = reinterpret_cast<SizeConstraints*>(dwRefData);
        if (c->minW > 0) mmi->ptMinTrackSize.x = c->minW;
        if (c->minH > 0) mmi->ptMinTrackSize.y = c->minH;
        if (c->maxW > 0) mmi->ptMaxTrackSize.x = c->maxW;
        if (c->maxH > 0) mmi->ptMaxTrackSize.y = c->maxH;
        return 0;
    }
    if (msg == WM_NCDESTROY)
    {
        s_sizeConstraints.erase(hwnd);
        RemoveWindowSubclass(hwnd, SizeConstraintsSubclassProc, uIdSubclass);
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

void setWebviewWindowMinSize(webview::webview& wv, int minWidth, int minHeight)
{
    auto hwnd = getHandleFromWebview(wv);
    if (!hwnd) return;

    auto& c = s_sizeConstraints[hwnd];
    c.minW = minWidth;
    c.minH = minHeight;

    SetWindowSubclass(hwnd, SizeConstraintsSubclassProc, 3, reinterpret_cast<DWORD_PTR>(&s_sizeConstraints[hwnd]));
}

void setWebviewWindowMaxSize(webview::webview& wv, int maxWidth, int maxHeight)
{
    auto hwnd = getHandleFromWebview(wv);
    if (!hwnd) return;

    auto& c = s_sizeConstraints[hwnd];
    c.maxW = maxWidth;
    c.maxH = maxHeight;

    SetWindowSubclass(hwnd, SizeConstraintsSubclassProc, 3, reinterpret_cast<DWORD_PTR>(&s_sizeConstraints[hwnd]));
}

void moveWebviewWindowTo(webview::webview& wv, int x, int y)
{
    auto hwnd = getHandleFromWebview(wv);
    if (!hwnd) return;

    SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

void moveWebviewWindowBy(webview::webview& wv, int deltaX, int deltaY)
{
    auto hwnd = getHandleFromWebview(wv);
    if (!hwnd) return;

    RECT rect;
    GetWindowRect(hwnd, &rect);
    SetWindowPos(hwnd, nullptr, rect.left + deltaX, rect.top + deltaY, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

void setWebviewWindowResizable(webview::webview& wv, bool resizable)
{
    auto hwnd = getHandleFromWebview(wv);
    if (!hwnd) return;

    LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);

    if (resizable)
        style |= WS_THICKFRAME;
    else
        style &= ~WS_THICKFRAME;

    SetWindowLongPtr(hwnd, GWL_STYLE, style);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}

void setWebviewWindowAlwaysOnTop(webview::webview& wv, bool alwaysOnTop)
{
    auto hwnd = getHandleFromWebview(wv);
    if (!hwnd) return;

    HWND insertAfter = alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST;
    SetWindowPos(hwnd, insertAfter, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}

void maximizeWebviewWindow(webview::webview& wv)
{
    auto hwnd = getHandleFromWebview(wv);
    if (!hwnd) return;
    ShowWindow(hwnd, SW_MAXIMIZE);
}

void minimizeWebviewWindow(webview::webview& wv)
{
    auto hwnd = getHandleFromWebview(wv);
    if (!hwnd) return;
    ShowWindow(hwnd, SW_MINIMIZE);
}

void restoreWebviewWindow(webview::webview& wv)
{
    auto hwnd = getHandleFromWebview(wv);
    if (!hwnd) return;
    ShowWindow(hwnd, SW_RESTORE);
}

void closeWebviewWindow(webview::webview& wv)
{
    auto hwnd = getHandleFromWebview(wv);
    if (!hwnd) return;
    PostMessage(hwnd, WM_CLOSE, 0, 0);
}

void showWebviewWindow(webview::webview& wv)
{
    auto hwnd = getHandleFromWebview(wv);
    if (!hwnd) return;
    ShowWindow(hwnd, SW_SHOW);
}

void hideWebviewWindow(webview::webview& wv)
{
    auto hwnd = getHandleFromWebview(wv);
    if (!hwnd) return;
    ShowWindow(hwnd, SW_HIDE);
}

void getWebviewWindowSize(const webview::webview& wv, int& width, int& height)
{
    auto hwnd = getHandleFromWebview(wv);
    if (!hwnd) return;

    RECT rect;
    GetClientRect(hwnd, &rect);
    width = rect.right - rect.left;
    height = rect.bottom - rect.top;
}

void getWebviewWindowPosition(const webview::webview& wv, int& x, int& y)
{
    auto hwnd = getHandleFromWebview(wv);
    if (!hwnd) return;

    RECT rect;
    GetWindowRect(hwnd, &rect);
    x = rect.left;
    y = rect.top;
}

void isWebviewWindowResizable(const webview::webview& wv, bool& resizable)
{
    auto hwnd = getHandleFromWebview(wv);
    if (!hwnd) return;

    LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
    resizable = (style & WS_THICKFRAME) != 0;
}

void isWebviewWindowAlwaysOnTop(const webview::webview& wv, bool& alwaysOnTop)
{
    auto hwnd = getHandleFromWebview(wv);
    if (!hwnd) return;

    LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    alwaysOnTop = (exStyle & WS_EX_TOPMOST) != 0;
}

void isWebviewWindowMaximized(const webview::webview& wv, bool& maximized)
{
    auto hwnd = getHandleFromWebview(wv);
    if (!hwnd) return;

    maximized = IsZoomed(hwnd) != 0;
}

void isWebviewWindowMinimized(const webview::webview& wv, bool& minimized)
{
    auto hwnd = getHandleFromWebview(wv);
    if (!hwnd) return;

    minimized = IsIconic(hwnd) != 0;
}

void isWebviewWindowRestored(const webview::webview& wv, bool& restored)
{
    auto hwnd = getHandleFromWebview(wv);
    if (!hwnd) return;

    restored = !IsZoomed(hwnd) && !IsIconic(hwnd);
}

void isWebviewWindowHidden(const webview::webview& wv, bool& hidden)
{
    auto hwnd = getHandleFromWebview(wv);
    if (!hwnd) return;

    hidden = !IsWindowVisible(hwnd);
}

#endif // _WIN32