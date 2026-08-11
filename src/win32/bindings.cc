#ifdef _WIN32

#include <windows.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <wrl/client.h>
#include <string>
#include <sstream>

#include "lib/json/json.h"
#include "../webviewWindow.h"

struct Handler : ICoreWebView2WebMessageReceivedEventHandler {
    ULONG ref = 1;
    std::function<void(const std::string&)> onMessageReceived;
    Handler(std::function<void(const std::string&)> fn) : onMessageReceived(fn) { }

    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* eventArgs) override {
        LPWSTR raw = nullptr;
        eventArgs->get_WebMessageAsJson(&raw);
        std::wstring wmsg(raw);
        CoTaskMemFree(raw);
        std::string msg(wmsg.begin(), wmsg.end());
        onMessageReceived(msg);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void**) override { return E_NOINTERFACE; }
    ULONG   STDMETHODCALLTYPE AddRef()  override { return ++ref; }
    ULONG   STDMETHODCALLTYPE Release() override { if (--ref == 0) { delete this; return 0; } return ref; }
};

void WebviewWindow::setupBindingsListener()
{
    if (!m_windowHandle.controller) return;

    ICoreWebView2* webview = nullptr;
    if (FAILED(m_windowHandle.controller->get_CoreWebView2(&webview)) || !webview)
    {
        return;
    }

    auto* handler = new Handler(
        [this](const std::string& msg)
        {
            JsonArgsVector argsJson;
            WebviewWindow::parseWebviewReq(msg, argsJson);

            std::string name = argsJson.back().asString();
            argsJson.pop_back();
            int seq = argsJson.back().asInt();
            argsJson.pop_back();

            auto it = m_bindings.find(name);
            if (it != m_bindings.end())
            {
                Json::Value response;
                response["ack"] = seq;

                try
                {
                    Json::Value  result = it->second(argsJson);
                    response["status"] = "success";
                    response["result"] = result;
                }
                catch (const std::exception& e)
                {
                    response["status"] = "error";
                    response["error"] = e.what();
                }

                emit("bindingResult", response);
            }
        }
    );

    webview->add_WebMessageReceived(handler, nullptr);
    handler->Release();
    webview->Release();
}

void WebviewWindow::exposeFunctionToWebview(const std::string& name, std::function<Json::Value(const JsonArgsVector&)> fn)
{
    ICoreWebView2* webview = nullptr;
    m_windowHandle.controller->get_CoreWebView2(&webview);

    std::string functionCreationString =
        "if (!window.webview) window.webview = {};"
        "window.webview['" + name + "'] = (...args) => {"
            "args.push('" + name + "');"
            "window.chrome.webview.postMessage(args);"
        "};";

    evalOnDocumentCreated(functionCreationString);
    m_bindings[name] = fn;
}

#endif // _WIN32