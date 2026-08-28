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
#define DWMSBT_NONE 1 /// No backdrop
#endif

#ifndef DWMSBT_MAINWINDOW
#define DWMSBT_MAINWINDOW 2 /// Mica
#endif

#ifndef DWMSBT_TRANSIENTWINDOW
#define DWMSBT_TRANSIENTWINDOW 3 /// Acrylic
#endif

#define WINDOWSTATE_UNKNOWN   0
#define WINDOWSTATE_MAXIMIZED 1
#define WINDOWSTATE_MINIMIZED 2
#define WINDOWSTATE_RESTORED  3

#define WINDOW_STYLE_PROC_ID 100


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
        WS_EX_TRANSPARENT,
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

            if (IsZoomed(hwnd))
            {
                HMONITOR hmon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY);
                MONITORINFO mi = {};
                mi.cbSize = sizeof(mi);
                GetMonitorInfo(hmon, &mi);
                params->rgrc[0].top = mi.rcWork.top;
            }
            else
            {
                params->rgrc[0].top = originalTop;
            }
            return 0;
        }

        // Handle window resizing to adjust the top resizer overlay's width when the window size changes.
        case WM_SIZE:
        {
            HWND hResizer = getTopResizerOverlay(hwnd);
            if (hResizer)
            {
                int width = LOWORD(lParam);
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


// -------- WebviewWindow Implementation ----------------------------------------------------------


void WebviewWindow::setBackdrop(WindowBackdropEffect backdrop)
{
    MARGINS margins = { -1, -1, -1, -1 };
    DwmExtendFrameIntoClientArea(m_windowHandle.hwnd, &margins);

    DWORD attr;
    switch (backdrop)
    {
        case WindowBackdropEffect::Solid:
            attr = DWMSBT_MAINWINDOW;
            break;
        case WindowBackdropEffect::Vibrant:
            attr = DWMSBT_TRANSIENTWINDOW;
            break;
        case WindowBackdropEffect::Transparent:
            attr = DWMSBT_NONE;
            break;

        default:
            break;
    }

    DwmSetWindowAttribute(m_windowHandle.hwnd, static_cast<DWMWINDOWATTRIBUTE>(DWMWA_SYSTEMBACKDROP_TYPE), &attr, sizeof(backdrop));
    SetWindowPos(m_windowHandle.hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    RedrawWindow(m_windowHandle.hwnd, nullptr, nullptr, RDW_UPDATENOW | RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN);

}

void WebviewWindow::setFrame(WindowFrameStyle frameStyle)
{
    LONG_PTR style = GetWindowLongPtr(m_windowHandle.hwnd, GWL_STYLE);

    switch (frameStyle)
    {
        case WindowFrameStyle::All:
            style |= WS_THICKFRAME | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
            removeTopResizerOverlay(m_windowHandle.hwnd);
            RemoveWindowSubclass(m_windowHandle.hwnd, StyleSubclassProc, WINDOW_STYLE_PROC_ID);
            break;

        case WindowFrameStyle::NoTitlebar:
            style |= WS_THICKFRAME | WS_CAPTION | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
            style &= ~WS_SYSMENU;
            createTopResizerOverlay(m_windowHandle.hwnd);
            SetWindowSubclass(m_windowHandle.hwnd, StyleSubclassProc, WINDOW_STYLE_PROC_ID, 0);
            break;

        case WindowFrameStyle::None:
            style &= ~(WS_THICKFRAME | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
            removeTopResizerOverlay(m_windowHandle.hwnd);
            RemoveWindowSubclass(m_windowHandle.hwnd, StyleSubclassProc, WINDOW_STYLE_PROC_ID);
            break;

        default:
            break;
    }

    SetWindowLongPtr(m_windowHandle.hwnd, GWL_STYLE, style);
    SetWindowPos(m_windowHandle.hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    RedrawWindow(m_windowHandle.hwnd, nullptr, nullptr, RDW_UPDATENOW | RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN);
}

#endif // _WIN32