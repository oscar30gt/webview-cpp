#include <sstream>
#include <functional>
#include <iostream>
#include "webviewWindow.h"

// -------- Constructors & Destructor -----------------------------------------------------------

// Main constructor that initializes the webview window with specified width and height.
WebviewWindow::WebviewWindow(int width, int height)
#ifdef DEBUG
    : m_webview(true, nullptr)
#else
    : m_webview(false, nullptr)
#endif
{
    m_webview.set_title("My Window");
    m_webview.set_size(width, height, WEBVIEW_HINT_NONE);
    setupPlatformSpecificWindowListeners(this);
    setupPlatformSpecificBindings(this);

    m_webview.bind(
        "webview:customEvent",
        [this](const std::string& req) -> std::string
        {
            JsonArgsVector args;
            if (parseWebviewReq(req, args))
            {
                std::string name = args[0].asString();
                args.erase(args.begin());
                this->emit(name, args);
            }

            return R"({"status":true})";
        }
    );

    setupBuiltinBindings();
}

// Derived constructor that relies on the main constructor and then navigates to a specified URL.
WebviewWindow::WebviewWindow(std::string url, int width, int height)
    : WebviewWindow(width, height)
{
    m_webview.navigate(url);
}

WebviewWindow::~WebviewWindow() = default;

// -------- Public Methods ------------------------------------------------------------------------

void WebviewWindow::run() { m_webview.run(); }
void WebviewWindow::terminate() { m_webview.terminate(); }

void WebviewWindow::emit(const std::string& eventName, const JsonArgsVector& args)
{
    // Notify the webview window
    std::ostringstream js;
    js << "window.dispatchEvent(new CustomEvent('webview-event', { detail: {"
        << "eventName: '" << eventName << "', args: [";

    for (auto arg : args)
    {
        js << arg.toStyledString() << ",";
    }

    js << "]}}));";
    m_webview.eval(js.str());

    // Notify registered listeners in the C++ context.
    auto range = m_listeners.equal_range(eventName);
    for (auto it = range.first; it != range.second; ++it)
    {
        it->second(args);
    }
}

void WebviewWindow::on(const std::string& channel, EventCallback callback)
{
    m_listeners.emplace(channel, callback);
}

void WebviewWindow::setStyle(WindowStyle style)
{
    setWebviewWindowStyle(m_webview, style);
}

void WebviewWindow::setSize(int width, int height) { m_webview.set_size(width, height, WEBVIEW_HINT_NONE); }
void WebviewWindow::setTitle(const std::string& title) { m_webview.set_title(title); }

// Platform-specific
void WebviewWindow::setResizable(bool resizable) { setWebviewWindowResizable(m_webview, resizable); }
void WebviewWindow::setAlwaysOnTop(bool alwaysOnTop) { setWebviewWindowAlwaysOnTop(m_webview, alwaysOnTop); }
void WebviewWindow::setMinSize(int minWidth, int minHeight) { setWebviewWindowMinSize(m_webview, minWidth, minHeight); }
void WebviewWindow::setMaxSize(int maxWidth, int maxHeight) { setWebviewWindowMaxSize(m_webview, maxWidth, maxHeight); }
void WebviewWindow::moveTo(int x, int y) { moveWebviewWindowTo(m_webview, x, y); }
void WebviewWindow::moveBy(int deltaX, int deltaY) { moveWebviewWindowBy(m_webview, deltaX, deltaY); }
void WebviewWindow::maximize() { maximizeWebviewWindow(m_webview); }
void WebviewWindow::minimize() { minimizeWebviewWindow(m_webview); }
void WebviewWindow::restore() { restoreWebviewWindow(m_webview); }
void WebviewWindow::close() { closeWebviewWindow(m_webview); }
void WebviewWindow::show() { showWebviewWindow(m_webview); }
void WebviewWindow::hide() { hideWebviewWindow(m_webview); }

WindowSize WebviewWindow::getSize() const
{
    int width, height;
    getWebviewWindowSize(m_webview, width, height);
    return { width, height };
}

WindowPosition WebviewWindow::getPosition() const
{
    int x, y;
    getWebviewWindowPosition(m_webview, x, y);
    return { x, y };
}

bool WebviewWindow::isResizable() const
{
    bool resizable;
    isWebviewWindowResizable(m_webview, resizable);
    return resizable;
}

bool WebviewWindow::isAlwaysOnTop() const
{
    bool alwaysOnTop;
    isWebviewWindowAlwaysOnTop(m_webview, alwaysOnTop);
    return alwaysOnTop;
}

bool WebviewWindow::isMaximized() const
{
    bool maximized;
    isWebviewWindowMaximized(m_webview, maximized);
    return maximized;
}

bool WebviewWindow::isMinimized() const
{
    bool minimized;
    isWebviewWindowMinimized(m_webview, minimized);
    return minimized;
}

bool WebviewWindow::isRestored() const
{
    bool restored;
    isWebviewWindowRestored(m_webview, restored);
    return restored;
}

bool WebviewWindow::isHidden() const
{
    bool hidden;
    isWebviewWindowHidden(m_webview, hidden);
    return hidden;
}


// -------- Private Methods -----------------------------------------------------------------------


void WebviewWindow::setupBuiltinBindings()
{
    // maximizeWindow(), minimizeWindow(), restoreWindow(), closeWindow(), showWindow(), hideWindow()
    m_webview.bind("webview:maximize", [this](const std::string& /* req */) { this->maximize(); return "";});
    m_webview.bind("webview:minimize", [this](const std::string& /* req */) { this->minimize(); return "";});
    m_webview.bind("webview:restore", [this](const std::string& /* req */) { this->restore(); return "";});
    m_webview.bind("webview:close", [this](const std::string& /* req */) { this->close(); return "";});
    m_webview.bind("webview:show", [this](const std::string& /* req */) { this->show(); return "";});
    m_webview.bind("webview:hide", [this](const std::string& /* req */) { this->hide(); return "";});

    // setWindowTitle(title: string);
    m_webview.bind("webview:setTitle", [this](const std::string& req) {
        JsonArgsVector args;
        parseWebviewReq(req, args);
        this->setTitle(args[0].asString());
        return "";
    });

    // setWindowResizable(resizable: boolean);
    m_webview.bind("webview:setResizable",
        [this](const std::string& req) {
            this->setResizable(req == "[true]");
            return "";
        }
    );

    // setWindowAlwaysOnTop(alwaysOnTop: boolean);
    m_webview.bind("webview:setAlwaysOnTop",
        [this](const std::string& req) {
            this->setAlwaysOnTop(req == "[true]");
            return "";
        }
    );

    // setWindowSize(width: number, height: number);
    m_webview.bind("webview:setSize",
        [this](const std::string& req) {
            int w, h;
            sscanf(req.c_str(), "[%d,%d]", &w, &h);
            this->setSize(w, h);
            return "";
        }
    );

    // setWindowMinSize(width: number, height: number);
    m_webview.bind("webview:setMinSize",
        [this](const std::string& req) {
            int w, h;
            sscanf(req.c_str(), "[%d,%d]", &w, &h);
            this->setMinSize(w, h);
            return "";
        }
    );

    // setWindowMaxSize(width: number, height: number);
    m_webview.bind("webview:setMaxSize",
        [this](const std::string& req) {
            int w, h;
            sscanf(req.c_str(), "[%d,%d]", &w, &h);
            this->setMaxSize(w, h);
            return "";
        }
    );

    // moveWindowTo(x: number, y: number);
    m_webview.bind("webview:moveTo",
        [this](const std::string& req) {
            int x, y;
            sscanf(req.c_str(), "[%d,%d]", &x, &y);
            this->moveTo(x, y);
            return "";
        }
    );

    // moveWindowBy(deltaX: number, deltaY: number);
    m_webview.bind("webview:moveBy",
        [this](const std::string& req) {
            int deltaX, deltaY;
            sscanf(req.c_str(), "[%d,%d]", &deltaX, &deltaY);
            this->moveBy(deltaX, deltaY);
            return "";
        }
    );

    // getWindowSize(): [number, number];
    m_webview.bind("webview:getSize",
        [this](const std::string& /* req */) {
            WindowSize size = this->getSize();
            std::ostringstream oss;
            oss << "{\"width\":" << size.width << ",\"height\":" << size.height << "}";
            return oss.str();
        }
    );

    // getWindowPosition(): [number, number];
    m_webview.bind("webview:getPosition",
        [this](const std::string& /* req */) {
            WindowPosition pos = this->getPosition();
            std::ostringstream oss;
            oss << "{\"x\":" << pos.x << ",\"y\":" << pos.y << "}";
            return oss.str();
        }
    );

    // isResizable(): boolean;
    m_webview.bind("webview:isResizable",
        [this](const std::string& /* req */) {
            bool resizable = this->isResizable();
            return resizable ? "true" : "false";
        }
    );

    // isAlwaysOnTop(): boolean;
    m_webview.bind("webview:isAlwaysOnTop",
        [this](const std::string& /* req */) {
            bool alwaysOnTop = this->isAlwaysOnTop();
            return alwaysOnTop ? "true" : "false";
        }
    );

    // isMaximized(): boolean;
    m_webview.bind("webview:isMaximized",
        [this](const std::string& /* req */) {
            bool maximized = this->isMaximized();
            return maximized ? "true" : "false";
        }
    );

    // isMinimized(): boolean;
    m_webview.bind("webview:isMinimized",
        [this](const std::string& /* req */) {
            bool minimized = this->isMinimized();
            return minimized ? "true" : "false";
        }
    );

    // isRestored(): boolean;
    m_webview.bind("webview:isRestored",
        [this](const std::string& /* req */) {
            bool restored = this->isRestored();
            return restored ? "true" : "false";
        }
    );

    // isHidden(): boolean;
    m_webview.bind("webview:isHidden",
        [this](const std::string& /* req */) {
            bool hidden = this->isHidden();
            return hidden ? "true" : "false";
        }
    );
}

// -------- Helpers -----------------------------------------------------------------------------

bool parseWebviewReq(
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