#pragma once

#include <string>
#include <vector>
#include <functional>
#include <concepts>
#include <type_traits>
#include <iostream>

#include "lib/webview.h"
#include "lib/json/json.h"

// -------- Interface and platform-specific functions ---------------------------------------------

/// Represents a window that contains a webview. This class encapsulates the creation, management
/// and interaction with a single webview window. This class uses webview.h to create and manage
/// multi-platform webview windows.
class WebviewWindow;

/// Type alias for event and binding arguments, represented as a vector of strings.
using JsonArgsVector = std::vector<Json::Value>;

// Type alias for event callback functions, which take a const reference to JsonArgsVector as an argument.
using EventCallback = std::function<void(const JsonArgsVector& args)>;

/// Supported window styles. Styles might vary across platforms.
/// @note For styles other than Native, the native titlebar will not be displayed. Thus,
/// a custom titlebar and window controls should be implemented in the webview content if
/// user is expected to interact with the window (drag, minimize, maximize, etc.).
enum class WindowStyle
{
    Native,           // Native frame and title bar, resizable
    Frameless,        // No frame, no title bar, resizable
    FramelessVibrant, // Framaless window with blur effect (acrylic on Windows, vibrancy on macOS)
    Transparent       // No frame, no resize, transparent window "floating"
};

/// Size of a window, represented by width and height.
struct WindowSize {
    int width;
    int height;
};

/// Position of a window, represented by x and y coordinates.
struct WindowPosition {
    int x;
    int y;
};

/// Concept to check if a function is bindable as a callback for WebviewWindow events.
template <typename F>
concept BindableCallback =
std::is_invocable_r_v<void, F> ||
std::is_invocable_r_v<void, F, const std::vector<Json::Value>&> ||
std::is_invocable_r_v<void, F, WebviewWindow&, const std::vector<Json::Value>&> ||
std::is_invocable_r_v<Json::Value, F> ||
std::is_invocable_r_v<Json::Value, F, const std::vector<Json::Value>&> ||
std::is_invocable_r_v<Json::Value, F, WebviewWindow&, const std::vector<Json::Value>&>;

/// Parses a JSON-formatted string originated from a webview binding request.
/// @param req The JSON-formatted request string to parse, which is expected to be an array of arguments.
/// @param outArgs A reference to a vector of strings where the extracted arguments will be stored.
/// @returns true if parsing was successful; false otherwise.
bool parseWebviewReq(
    const std::string& req,
    JsonArgsVector& outArgs
);

/// Registers platform-specific bindings for the webview instance, for callbacks that directly 
/// interact with the OS.
/// @param wv The webview instance for which the bindings are to be registered.
/// @note This function is meant to be called once during the initialization of the webview window
void setupPlatformSpecificBindings(WebviewWindow* window);

/// Sets up platform-specific window listeners for native window events, such as resize, move
/// or close. Events should trigger notify*() methods on the WebviewWindow instance.
/// @param window The WebviewWindow instance for which the listeners are to be set up.
/// @note This function is meant to be called once during the initialization of the webview window
void setupPlatformSpecificWindowListeners(WebviewWindow* window);

/// Sets the window style for the given webview instance based on the specified WindowStyle enum.
/// @param wv The webview instance for which the window style is to be set.
/// @param style The desired window style, specified as a WindowStyle enum value.
void setWebviewWindowStyle(webview::webview& wv, const WindowStyle style);

void setWebviewWindowResizable(webview::webview& wv, bool resizable);
void setWebviewWindowAlwaysOnTop(webview::webview& wv, bool alwaysOnTop);
void setWebviewWindowMinSize(webview::webview& wv, int minWidth, int minHeight);
void setWebviewWindowMaxSize(webview::webview& wv, int maxWidth, int maxHeight);
void moveWebviewWindowTo(webview::webview& wv, int x, int y);
void moveWebviewWindowBy(webview::webview& wv, int deltaX, int deltaY);
void maximizeWebviewWindow(webview::webview& wv);
void minimizeWebviewWindow(webview::webview& wv);
void restoreWebviewWindow(webview::webview& wv);
void closeWebviewWindow(webview::webview& wv);
void showWebviewWindow(webview::webview& wv);
void hideWebviewWindow(webview::webview& wv);

void getWebviewWindowSize(const webview::webview& wv, int& width, int& height);
void getWebviewWindowPosition(const webview::webview& wv, int& x, int& y);
void isWebviewWindowResizable(const webview::webview& wv, bool& resizable);
void isWebviewWindowAlwaysOnTop(const webview::webview& wv, bool& alwaysOnTop);
void isWebviewWindowMaximized(const webview::webview& wv, bool& maximized);
void isWebviewWindowMinimized(const webview::webview& wv, bool& minimized);
void isWebviewWindowRestored(const webview::webview& wv, bool& restored);
void isWebviewWindowHidden(const webview::webview& wv, bool& hidden);


// -------- WebviewWindow class -------------------------------------------------------------------

class WebviewWindow
{

public:

    WebviewWindow(int width = 800, int height = 600);
    WebviewWindow(std::string url, int width = 800, int height = 600);
    ~WebviewWindow();

    // -------- Window lifecycle --------

    /// Runs the webview window, entering the main event loop. This function blocks the thread where 
    /// it is called until the window is closed.
    void run();

    /// Terminates the webview window, closing it and exiting the main event loop.
    void terminate();

    // -------- Events & Bindings --------

    /// Emits an event to the webview window.
    /// @param eventName The name of the event to emit.
    /// @param args Optional vector of JSON values containing additional data related to the event.
    void emit(const std::string& eventName, const JsonArgsVector& args = JsonArgsVector());

    /// Registers a callback function to be invoked when a certain event is triggered.
    /// @param eventName The name of the event to listen for.
    /// @param callback The callback function to be invoked when the event is triggered. The callback
    void on(const std::string& eventName, EventCallback callback);

    /// Binds a C++ function to a JavaScript function in the webview context.
    /// Bindings differ from events in that they are synchronous and only one callback can be registered
    /// for a given binding name. Thus, they can return a value to the JavaScript context.
    /// Bindings can be invoked via invoke() inside the webview.
    /// @tparam F The type of the function to bind. It must be a callable that returns either void or Json::Value. 
    /// Arguments can be none, a vector of Json::Value (args), or a reference to the WebviewWindow instance followed 
    /// by a vector of Json::Value (args).
    /// @param name The name of the binding, which will be used to call the function from JavaScript.
    /// @param fn The function to bind, which will be called when the binding is invoked from JavaScript.
    template <BindableCallback F>
    void bind(const std::string& name, F&& fn);

    // -------- Window Manipulation & Querying --------

    void setSize(int width, int height); /// Resizes the window to the specified width and height.
    void setTitle(const std::string& title); /// Sets the title of the window (taskbar, mission control, etc.).
    void setResizable(bool resizable); /// Sets whether the window can be resized by the user.
    void setAlwaysOnTop(bool alwaysOnTop); /// Sets whether the window should always stay on top of other windows.
    void setMinSize(int minWidth, int minHeight); /// Sets the minimum size of the window it can be resized to.
    void setMaxSize(int maxWidth, int maxHeight); /// Sets the maximum size of the window it can be resized to.
    void moveTo(int x, int y); /// Moves the window to the specified screen coordinates (x, y).
    void moveBy(int deltaX, int deltaY); /// Moves the window by the specified delta (deltaX, deltaY).
    void maximize(); /// Maximizes the window
    void minimize(); /// Minimizes the window
    void restore(); /// Restores (unmaximizes) the window
    void close(); /// Closes the window
    void show(); /// Shows the window
    void hide(); /// Hides the window

    WindowSize getSize() const; /// The current size of the window.
    WindowPosition getPosition() const; /// The current position of the window.
    bool isResizable() const; /// Whether the window is resizable.
    bool isAlwaysOnTop() const; /// Whether the window is always on top.
    bool isMaximized() const; /// Whether the window is maximized.
    bool isMinimized() const; /// Whether the window is minimized.
    bool isRestored() const; /// Whether the window is restored (not maximized or minimized).
    bool isHidden() const; /// Whether the window is currently hidden.

    // -------- Misc --------

    /// Sets the style of the webview window based on the specified WindowStyle enum. 
    /// The resulting window appearance might vary across platforms.
    /// @param style Style variant to set.
    void setStyle(WindowStyle style);

protected:

    // Webview instance. Wrapped inside the WebviewWindow class to provide a higher-level
    // interface for window management and event handling.
    webview::webview m_webview;

    friend void setupPlatformSpecificBindings(WebviewWindow* window);
    friend void setupPlatformSpecificWindowListeners(WebviewWindow* window);

private:

    /// Listener map for registered event callbacks.
    /// Each channel can have multiple callbacks associated with it. 
    std::unordered_multimap<std::string, EventCallback> m_listeners;

    /// Sets up built-in bindings for the webview window, which are used for communication
    /// of built-in events, such as window control actions (maximize, minimize, close, etc.)
    /// or querying window state (size, position, etc.) from the frontend.
    void setupBuiltinBindings();

}; // class WebviewWindow

// -------- Implementation (template methods) -----------------------------------------------------

namespace {

    /// Compile-time callback selection based on the signature of the provided function.
    /// Callbacks that return void will be wrapped to return a null JSON value.
    template <BindableCallback F>
    auto makeUnifiedBindingCallback(F&& fn) {
        return [fn = std::forward<F>(fn)]
        (WebviewWindow& win, const JsonArgsVector& args) -> Json::Value
        {
            // (win, args) -> Json::Value
            if constexpr (std::is_invocable_r_v<Json::Value, F, WebviewWindow&, const JsonArgsVector&>)
                return fn(win, args);

            // (win, args) -> void
            if constexpr (std::is_invocable_r_v<void, F, WebviewWindow&, const JsonArgsVector&>)
                return (fn(win, args), Json::Value::nullRef);

            // (args) -> Json::Value
            if constexpr (std::is_invocable_r_v<Json::Value, F, const JsonArgsVector&>)
                return fn(args);

            // (args) -> void
            if constexpr (std::is_invocable_r_v<void, F, const JsonArgsVector&>)
                return (fn(args), Json::Value::nullRef);

            // () -> Json::Value
            if constexpr (std::is_invocable_r_v<Json::Value, F>)
                return fn();

            // () -> void
            if constexpr (std::is_invocable_r_v<void, F>)
                return (fn(), Json::Value::nullRef);
        };
    }
} // namespace

template <BindableCallback F>
void WebviewWindow::bind(const std::string& name, F&& fn) {
    auto callbackFn = makeUnifiedBindingCallback(std::forward<F>(fn));

    // A binding is a synchronous function call from JavaScript to C++.
    // Binding identifiers are namespaced with "binding:" to avoid conflicts with other identifiers.
    m_webview.bind(
        "binding:" + name,
        [this, callbackFn]
        (const std::string& req) -> std::string
        {
            std::ostringstream ss;
            JsonArgsVector args;
            if (parseWebviewReq(req, args))
            {
                Json::Value ret = callbackFn(*this, args);
                ss << std::boolalpha << R"({"status":true,"value":)" << ret.toStyledString() << "}";
                return ss.str();
            }

            return R"({"status":false,"error":"Invalid request format"})";
        }
    );
}