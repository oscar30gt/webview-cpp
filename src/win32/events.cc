#ifdef _WIN32

#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>

#include "../webviewWindow.h"

#define WINDOWSTATE_UNKNOWN   0
#define WINDOWSTATE_MAXIMIZED 1
#define WINDOWSTATE_MINIMIZED 2
#define WINDOWSTATE_RESTORED  3

#define EVENT_SUBCLASS_ID 20

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
            Json::Value sizeChangeDetail;
            sizeChangeDetail["width"] = LOWORD(lParam);
            sizeChangeDetail["height"] = HIWORD(lParam);

            win->emit("webview:resized", sizeChangeDetail);
            break;
        }

        case WM_MOVE:
        {
            // To capture negative coordinates on secondary monitors,
            // the macros from <windowsx.h> are used over direct LOWORD/HIWORD.
            Json::Value moveDetail;
            moveDetail["x"] = GET_X_LPARAM(lParam);
            moveDetail["y"] = GET_Y_LPARAM(lParam);

            win->emit("webview:moved", moveDetail);
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

void WebviewWindow::subscribeToWindowEvents()
{
    SetWindowSubclass(m_windowHandle.hwnd, EventSubclassProc, EVENT_SUBCLASS_ID, (DWORD_PTR)this);
}

#endif // _WIN32