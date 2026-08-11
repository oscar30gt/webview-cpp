/// webviewWindow.cc
/// common implementation logic for the WebviewWindow class, shared across platforms.

#include <sstream>
#include <functional>
#include <iostream>

#include "webviewWindow.h"

// -------- Constructors & Destructor -------------------------------------------------------------

WebviewWindow::WebviewWindow(int width, int height)
    : m_windowHandle(createWebviewWindow(width, height)) 
{
    setupBindingsListener();
    subscribeToWindowEvents();

    exposeFunctionToWebview("webview:setTitle", [this](const JsonArgsVector& args) {
        setTitle(args[0].asString());
        return Json::nullValue;
    });

    exposeFunctionToWebview("webview:maximize", [this](const JsonArgsVector& /* args */) {
        maximize();
        return Json::nullValue;
    });

    exposeFunctionToWebview("webview:minimize", [this](const JsonArgsVector& /* args */) {
        minimize();
        return Json::nullValue;
    });

    exposeFunctionToWebview("webview:restore", [this](const JsonArgsVector& /* args */) {
        restore();
        return Json::nullValue;
    });

    exposeFunctionToWebview("webview:close", [this](const JsonArgsVector& /* args */) {
        close();
        return Json::nullValue;
    });

    exposeFunctionToWebview("webview:show", [this](const JsonArgsVector& /* args */) {
        show();
        return Json::nullValue;
    });

    exposeFunctionToWebview("webview:hide", [this](const JsonArgsVector& /* args */) {
        hide();
        return Json::nullValue;
    });

    exposeFunctionToWebview("webview:setResizable", [this](const JsonArgsVector& args) {
        setResizable(args[0].asBool());
        return Json::nullValue;
    });

    exposeFunctionToWebview("webview:setAlwaysOnTop", [this](const JsonArgsVector& args) {
        setAlwaysOnTop(args[0].asBool());
        return Json::nullValue;
    });

    exposeFunctionToWebview("webview:setSize", [this](const JsonArgsVector& args) {
        setSize(args[0].asInt(), args[1].asInt());
        return Json::nullValue;
    });

    exposeFunctionToWebview("webview:setMinSize", [this](const JsonArgsVector& args) {
        setMinSize(args[0].asInt(), args[1].asInt());
        return Json::nullValue;
    });

    exposeFunctionToWebview("webview:setMaxSize", [this](const JsonArgsVector& args) {
        setMaxSize(args[0].asInt(), args[1].asInt());
        return Json::nullValue;
    });

    exposeFunctionToWebview("webview:setPosition", [this](const JsonArgsVector& args) {
        setPosition(args[0].asInt(), args[1].asInt());
        return Json::nullValue;
    });

    exposeFunctionToWebview("webview:move", [this](const JsonArgsVector& args) {
        move(args[0].asInt(), args[1].asInt());
        return Json::nullValue;
    });

    exposeFunctionToWebview("webview:getSize", [this](const JsonArgsVector& /* args */) -> Json::Value {
        WindowSize size = getSize();
        Json::Value result;
        result["width"] = size.width;
        result["height"] = size.height;
        return result;
    });

    exposeFunctionToWebview("webview:getPosition", [this](const JsonArgsVector& /* args */) -> Json::Value {
        WindowPosition pos = getPosition();
        Json::Value result;
        result["x"] = pos.x;
        result["y"] = pos.y;
        return result;
    });

    exposeFunctionToWebview("webview:isResizable", [this](const JsonArgsVector& /* args */) -> Json::Value {
        return isResizable();
    });

    exposeFunctionToWebview("webview:isAlwaysOnTop", [this](const JsonArgsVector& /* args */) -> Json::Value {
        return isAlwaysOnTop();
    });

    exposeFunctionToWebview("webview:isMaximized", [this](const JsonArgsVector& /* args */) -> Json::Value {
        return isMaximized();
    });

    exposeFunctionToWebview("webview:isMinimized", [this](const JsonArgsVector& /* args */) -> Json::Value {
        return isMinimized();
    });

    exposeFunctionToWebview("webview:isRestored", [this](const JsonArgsVector& /* args */) -> Json::Value {
        return isRestored();
    });

    exposeFunctionToWebview("webview:isHidden", [this](const JsonArgsVector& /* args */) -> Json::Value {
        return isHidden();
    });
}

// Derived constructor that relies on the main constructor and then navigates to a specified URL.
WebviewWindow::WebviewWindow(std::string url, int width, int height)
    : WebviewWindow(width, height)
{
    navigate(url);
}

WebviewWindow::~WebviewWindow()
{
    destroyWebviewWindow(m_windowHandle);
}

// -------- Events --------------------------------------------------------------------------------

void WebviewWindow::emit(const std::string& eventName, const Json::Value& detail)
{
    // Notify the webview window
    std::ostringstream js;
    js << "window.dispatchEvent(new CustomEvent('webview', { detail: {"
        << "eventName: '" << eventName 
        << "', detail: " << detail.toStyledString() 
        << " }}));";

    eval(js.str());
}

// -------- Helpers -------------------------------------------------------------------------------

bool WebviewWindow::parseWebviewReq(
    const std::string& req, JsonArgsVector& outArgs
)
{
    Json::Value root;
    Json::CharReaderBuilder readerBuilder;
    std::string errs;
    std::istringstream stream(req);
    // Parse
    if (!Json::parseFromStream(readerBuilder, stream, &root, &errs) || !root.isArray())
        return false;

    // Iterate and extract arguments
    for (Json::Value::ArrayIndex i = 0; i < root.size(); ++i)
    {
        const Json::Value& val = root[i];
        outArgs.push_back(val);
    }
    return true;
}