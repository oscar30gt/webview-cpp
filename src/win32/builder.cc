#ifdef _WIN32

#include <windows.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <wrl/client.h>
#include <string>

#include "../webviewWindow.h"


// -------- Global State --------------------------------------------------------------------------


/// Function pointer type for the internal WebView2 environment creation function.
typedef HRESULT(STDMETHODCALLTYPE* CreateWebViewEnvironmentWithOptionsInternal_t)(
    bool, int, PCWSTR, IUnknown*,
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*);

static CreateWebViewEnvironmentWithOptionsInternal_t g_CreateEnvInternal = nullptr;
static HMODULE g_webview2_dll = nullptr;


// -------- .dll Loading & Unloading -------------------------------------------------------------- 


/// Loads the WebView2 runtime DLL if it is not already loaded.
/// @return true if the DLL is loaded and ready to use; false if the DLL could not be loaded.
bool LoadWebView2() {

    if (g_webview2_dll)
        return true;

    HKEY hKey;
    const wchar_t* subKey =
        L"SOFTWARE\\Microsoft\\EdgeUpdate\\ClientState\\"
        L"{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}";

    if (
        RegOpenKeyExW(HKEY_LOCAL_MACHINE, subKey, 0, KEY_READ | KEY_WOW64_32KEY, &hKey) != ERROR_SUCCESS &&
        RegOpenKeyExW(HKEY_CURRENT_USER, subKey, 0, KEY_READ | KEY_WOW64_32KEY, &hKey) != ERROR_SUCCESS
        ) return false;

    wchar_t ebwebview_path[MAX_PATH * 2] = {};
    DWORD size = sizeof(ebwebview_path);
    DWORD type = REG_SZ;
    LSTATUS status = RegQueryValueExW(hKey, L"EBWebView", nullptr, &type, (LPBYTE)ebwebview_path, &size);
    RegCloseKey(hKey);

    if (status != ERROR_SUCCESS)
        return false;

    std::wstring dll_path = ebwebview_path;
    if (!dll_path.empty() && dll_path.back() != L'\\')
    {
        dll_path += L'\\';
    }
    dll_path += L"EBWebView\\x64\\EmbeddedBrowserWebView.dll";

    g_webview2_dll = LoadLibraryW(dll_path.c_str());
    if (!g_webview2_dll)
        return false;

    g_CreateEnvInternal =
        (CreateWebViewEnvironmentWithOptionsInternal_t)GetProcAddress(
        g_webview2_dll, "CreateWebViewEnvironmentWithOptionsInternal");

    return g_CreateEnvInternal != nullptr;
}

/// Unloads the WebView2 runtime DLL if it is loaded.
void UnloadWebView2() {
    if (g_webview2_dll)
    {
        FreeLibrary(g_webview2_dll);
        g_webview2_dll = nullptr;
    }
}


// -------- Drag Region Support -------------------------------------------------------------------


/// Given a WebView2 controller, enables support for `-webkit-app-region: drag` in the web content
/// to drag the window when the user clicks and drags on those regions.
void enableAppRegionDrag(ICoreWebView2Controller* controller) {
    using Microsoft::WRL::ComPtr;

    ComPtr<ICoreWebView2> core;
    controller->get_CoreWebView2(&core);
    if (!core) return;

    ComPtr<ICoreWebView2Settings> settings;
    core->get_Settings(&settings);
    if (!settings) return;

    ComPtr<ICoreWebView2Settings9> settings9;
    if (SUCCEEDED(settings.CopyTo(IID_ICoreWebView2Settings9, &settings9)))
    {
        settings9->put_IsNonClientRegionSupportEnabled(TRUE);
    }
}


// -------- Controller & Environment Handlers -----------------------------------------------------


class ControllerHandler : public ICoreWebView2CreateCoreWebView2ControllerCompletedHandler {
    ULONG ref_count = 1;
    NativeWindowHandle* m_handle;
public:
    ControllerHandler(NativeWindowHandle* handle) : m_handle(handle) { }

    HRESULT STDMETHODCALLTYPE Invoke(HRESULT errorCode, ICoreWebView2Controller* controller) {
        if (FAILED(errorCode)) return errorCode;

        m_handle->controller = controller;
        controller->AddRef();
        controller->put_IsVisible(TRUE);
        enableAppRegionDrag(controller);

        // Set transparent background
        MARGINS margins = { -1, -1, -1, -1 };
        DwmExtendFrameIntoClientArea(m_handle->hwnd, &margins);
        SetClassLongPtr(m_handle->hwnd, GCLP_HBRBACKGROUND, reinterpret_cast<LONG_PTR>(GetStockObject(BLACK_BRUSH)));

        ICoreWebView2Controller2* controller2 = nullptr;
        if (SUCCEEDED(controller->QueryInterface(IID_ICoreWebView2Controller2, reinterpret_cast<void**>(&controller2))) && controller2)
        {
            COREWEBVIEW2_COLOR transparentColor = { 0, 0, 0, 0 };
            controller2->put_DefaultBackgroundColor(transparentColor);
            controller2->Release();
        }

        RECT bounds;
        GetClientRect(m_handle->hwnd, &bounds);
        controller->put_Bounds(bounds);

        m_handle->ready = true;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) {
        if (!ppvObject) return E_INVALIDARG;
        *ppvObject = nullptr;

        if (riid == IID_IUnknown || riid == IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler)
        {
            *ppvObject = this;
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() { return ++ref_count; }

    ULONG STDMETHODCALLTYPE Release() {
        ULONG count = --ref_count;
        if (count == 0) delete this;
        return count;
    }
};

class EnvironmentHandler : public ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler {
    ULONG ref_count = 1;
    NativeWindowHandle* m_handle;
public:
    EnvironmentHandler(NativeWindowHandle* handle) : m_handle(handle) { }

    HRESULT STDMETHODCALLTYPE Invoke(HRESULT errorCode, ICoreWebView2Environment* env) {
        if (FAILED(errorCode)) return errorCode;

        env->CreateCoreWebView2Controller(m_handle->hwnd, new ControllerHandler(m_handle));
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) {
        if (!ppvObject) return E_INVALIDARG;
        *ppvObject = nullptr;

        if (riid == IID_IUnknown || riid == IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler)
        {
            *ppvObject = this;
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() { return ++ref_count; }

    ULONG STDMETHODCALLTYPE Release() {
        ULONG count = --ref_count;
        if (count == 0) delete this;
        return count;
    }
};


// -------- Window Subclasses ---------------------------------------------------------------------


/// Main window procedure for the created window.
LRESULT CALLBACK MainSubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    NativeWindowHandle* h = (NativeWindowHandle*)dwRefData;

    switch (msg)
    {
        // Keep the WebView2 controller bounds in sync with the window size.
        case WM_SIZE:
            if (h->controller)
            {
                RECT bounds;
                GetClientRect(hWnd, &bounds);
                h->controller->put_Bounds(bounds);
            }
            break;

            // Handle window destruction and cleanup.
        case WM_DESTROY:
            if (h->controller)
            {
                h->controller->Close();
                h->controller->Release();
                h->controller = nullptr;
            }
            RemoveWindowSubclass(hWnd, MainSubclassProc, 0);
            PostQuitMessage(0);
            break;

        default:
            return DefSubclassProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}


// -------- WebviewWindow Implementation ----------------------------------------------------------


NativeWindowHandle createWebviewWindow(int width, int height) {
    NativeWindowHandle handle;
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    if (!LoadWebView2())
    {
        MessageBoxW(
            nullptr,
            L"WebView2 Runtime is not installed.\n\n"
            L"Please download it from:\n"
            L"https://developer.microsoft.com/en-us/microsoft-edge/webview2/",
            L"Error",
            MB_ICONERROR
        );
        throw std::runtime_error("WebView2 Runtime is not installed.");
    }

    const wchar_t CLASS_NAME[] = L"WebviewWindow";
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = DefWindowProc;
    wc.lpszClassName = CLASS_NAME;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wc);

    handle.hwnd = CreateWindowEx(
        0, CLASS_NAME, L"WebviewWindow", WS_OVERLAPPEDWINDOW,
        100, 100, width, height, NULL, NULL, GetModuleHandle(NULL), NULL
    );

    handle.controller = nullptr;

    SetWindowSubclass(handle.hwnd, MainSubclassProc, 0, (DWORD_PTR)&handle);

    wchar_t temp_path[MAX_PATH];
    GetTempPathW(MAX_PATH, temp_path);
    wcscat_s(temp_path, MAX_PATH, L"WebView2Temp");

    g_CreateEnvInternal(true, 0, temp_path, nullptr, new EnvironmentHandler(&handle));

    // Active wait loop to ensure the webview is ready before returning from the constructor.
    // During this loop, the message queue is processed so windows messages are handled and the
    // window is not recognized as unresponsive. Webview loading is asynchronous, but almost
    // instant. However, many methods depend on its initialization, so we wait until it is ready.
    MSG msg;
    while (!handle.ready && GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    // Show window now that WebView is ready
    ShowWindow(handle.hwnd, SW_SHOW);
    return handle;
}

void destroyWebviewWindow(NativeWindowHandle& handle) {
    if (handle.controller)
    {
        handle.controller->Close();
        handle.controller->Release();
        handle.controller = nullptr;
    }

    DestroyWindow(handle.hwnd);
    handle.hwnd = nullptr;
}

#endif