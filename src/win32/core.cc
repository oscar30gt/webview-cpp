#ifdef _WIN32

#include <windows.h>
#include <dwmapi.h>

#include "../webviewWindow.h"


// -------- Window lifecycle ----------------------------------------------------------------------

void WebviewWindow::run()
{
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    CoUninitialize();
}

void WebviewWindow::close()
{
    PostMessage(m_windowHandle.hwnd, WM_CLOSE, 0, 0);
}

void WebviewWindow::navigate(const std::string& url)
{
    ICoreWebView2* webview;
    m_windowHandle.controller->get_CoreWebView2(&webview);
    std::wstring wurl(url.begin(), url.end());
    webview->Navigate(wurl.c_str());
    webview->Release();
}

void WebviewWindow::eval(const std::string& jsCode)
{
    ICoreWebView2* webview;
    m_windowHandle.controller->get_CoreWebView2(&webview);
    std::wstring wjs(jsCode.begin(), jsCode.end());
    webview->ExecuteScript(wjs.c_str(), nullptr);
    webview->Release();
}

void WebviewWindow::evalOnDocumentCreated(const std::string& jsCode)
{
    ICoreWebView2* webview;
    m_windowHandle.controller->get_CoreWebView2(&webview);
    std::wstring wjs(jsCode.begin(), jsCode.end());
    webview->AddScriptToExecuteOnDocumentCreated(wjs.c_str(), nullptr);
    webview->Release();
}

// -------- Window sizing -------------------------------------------------------------------------

static LRESULT CALLBACK SizeConstraintsSubclassProc(
    HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    auto* c = reinterpret_cast<WindowSizeConstraints*>(dwRefData);

    if (msg == WM_GETMINMAXINFO)
    {
        auto* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
        if (c->minSize.width > 0) mmi->ptMinTrackSize.x = c->minSize.width;
        if (c->minSize.height > 0) mmi->ptMinTrackSize.y = c->minSize.height;
        if (c->maxSize.width > 0) mmi->ptMaxTrackSize.x = c->maxSize.width;
        if (c->maxSize.height > 0) mmi->ptMaxTrackSize.y = c->maxSize.height;
        return 0;
    }

    if (msg == WM_NCDESTROY)
    {
        RemoveWindowSubclass(hwnd, SizeConstraintsSubclassProc, uIdSubclass);
    }

    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

void WebviewWindow::setSize(int width, int height)
{
    SetWindowPos(m_windowHandle.hwnd, NULL, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER);
}

void WebviewWindow::setMinSize(int minWidth, int minHeight)
{
    m_sizeConstraints.minSize = { minWidth, minHeight };
    SetWindowSubclass(m_windowHandle.hwnd, SizeConstraintsSubclassProc, 30, (DWORD_PTR)&m_sizeConstraints);
}

void WebviewWindow::setMaxSize(int maxWidth, int maxHeight)
{
    m_sizeConstraints.maxSize = { maxWidth, maxHeight };
    SetWindowSubclass(m_windowHandle.hwnd, SizeConstraintsSubclassProc, 30, (DWORD_PTR)&m_sizeConstraints);
}

void WebviewWindow::setResizable(bool resizable)
{
    LONG_PTR style = GetWindowLongPtr(m_windowHandle.hwnd, GWL_STYLE);
    if (resizable)
        style |= WS_THICKFRAME;
    else
        style &= ~WS_THICKFRAME;
    SetWindowLongPtr(m_windowHandle.hwnd, GWL_STYLE, style);
    SetWindowPos(m_windowHandle.hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}

bool WebviewWindow::isResizable() const
{
    LONG_PTR style = GetWindowLongPtr(m_windowHandle.hwnd, GWL_STYLE);
    return (style & WS_THICKFRAME) != 0;
}


// -------- Window manipulation -------------------------------------------------------------------

void WebviewWindow::setTitle(const std::string& title)
{
    SetWindowText(m_windowHandle.hwnd, std::wstring(title.begin(), title.end()).c_str());
}

void WebviewWindow::setAlwaysOnTop(bool alwaysOnTop)
{
    HWND insertAfter = alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST;
    SetWindowPos(m_windowHandle.hwnd, insertAfter, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}

void WebviewWindow::setPosition(int x, int y)
{
    SetWindowPos(m_windowHandle.hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

void WebviewWindow::move(int deltaX, int deltaY)
{
    RECT rect;
    GetWindowRect(m_windowHandle.hwnd, &rect);
    SetWindowPos(m_windowHandle.hwnd, nullptr, rect.left + deltaX, rect.top + deltaY, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

void WebviewWindow::maximize()
{
    ShowWindow(m_windowHandle.hwnd, SW_MAXIMIZE);
}

void WebviewWindow::minimize()
{
    ShowWindow(m_windowHandle.hwnd, SW_MINIMIZE);
}

void WebviewWindow::restore()
{
    ShowWindow(m_windowHandle.hwnd, SW_RESTORE);
}

void WebviewWindow::show()
{
    ShowWindow(m_windowHandle.hwnd, SW_SHOW);
}

void WebviewWindow::hide()
{
    ShowWindow(m_windowHandle.hwnd, SW_HIDE);
}

WindowSize WebviewWindow::getSize() const
{
    RECT rect;
    GetWindowRect(m_windowHandle.hwnd, &rect);
    return { rect.right - rect.left, rect.bottom - rect.top };
}

WindowPosition WebviewWindow::getPosition() const
{
    RECT rect;
    GetWindowRect(m_windowHandle.hwnd, &rect);
    return { rect.left, rect.top };
}

bool WebviewWindow::isAlwaysOnTop() const
{
    LONG_PTR exStyle = GetWindowLongPtr(m_windowHandle.hwnd, GWL_EXSTYLE);
    return (exStyle & WS_EX_TOPMOST) != 0;
}

bool WebviewWindow::isMaximized() const
{
    return IsZoomed(m_windowHandle.hwnd);
}

bool WebviewWindow::isMinimized() const
{
    return IsIconic(m_windowHandle.hwnd);
}

bool WebviewWindow::isRestored() const
{
    return !IsZoomed(m_windowHandle.hwnd) && !IsIconic(m_windowHandle.hwnd);
}

bool WebviewWindow::isHidden() const
{
    return !IsWindowVisible(m_windowHandle.hwnd);
}


#endif