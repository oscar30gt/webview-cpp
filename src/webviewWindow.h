#pragma once

#include <string>
#include <vector>
#include <functional>
#include <concepts>
#include <type_traits>
#include <iostream>

#include "lib/json/json.h"


// -------- Types ---------------------------------------------------------------------------------


/// Represents a window that contains a webview. This class encapsulates the creation, management
/// and interaction with a single webview window. This class uses webview.h to create and manage
/// multi-platform webview windows.
class WebviewWindow;

/// A vector of JSON arguments. Typically used to receive arguments from webview binding requests.
using JsonArgsVector = std::vector<Json::Value>;

// Type alias for event callback functions, which take a const reference to JsonArgsVector as an argument.
using EventCallback = std::function<void(const JsonArgsVector& args)>;

/// Backdrop style of the window, which can affect its appearance.
/// Backdrop implementation may vary across platforms.
enum class WindowBackdropEffect
{
    Solid,
    Vibrant, /// Acrylic on Windows, Vibrancy on macOS
    Transparent
};

enum class WindowFrameStyle
{
    All, /// Standard window frame with title bar and borders
    NoTitlebar, /// Bordered window without title bar
    None /// Frameless window without title bar or borders
};

/// Size of a window, represented by width and height.
struct WindowSize
{
    int width;
    int height;
};

/// Position of a window, represented by x and y coordinates.
struct WindowPosition
{
    int x;
    int y;
};

/// Constraints for the size of a window.
/// If 0, constraints will not be enforced for that dimension.
/// Max size must be greater than or equal to min size for each dimension.
struct WindowSizeConstraints
{
    WindowSize minSize = { 0, 0 };
    WindowSize maxSize = { 0, 0 };
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


// -------- Platform-specific ---------------------------------------------------------------------


#ifdef _WIN32

#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN

#include "lib/WebView2.h" // amalgamate(skip)

/// Set of native properties for a window.
/// This structure varies across platforms and contains platform-specific
/// properties and handles for direct interaction with the system window system.
struct NativeWindowHandle {
    HWND hwnd;
    ICoreWebView2Controller* controller;
    bool ready = false;
};

#endif

#ifdef __APPLE__

/// NativeWindowHandle for macOS.
/// Stored as opaque void* pointers to avoid including Cocoa/WebKit headers
/// in the public platform-agnostic header.
struct NativeWindowHandle {
    void* nsWindow = nullptr;        ///< NSWindow* (retained)
    void* wkWebView = nullptr;       ///< WKWebView* (retained)
    void* windowDelegate = nullptr;  ///< NSWindowDelegate* (retained)
    void* scriptDelegate = nullptr;  ///< WKScriptMessageHandler* (retained)
    bool  ready = false;
};

#endif


// -------- Friend Functions ----------------------------------------------------------------------


/// Creates and initializes a window with a webview in it.
/// @param width Width of the window to create.
/// @param height Height of the window to create.
/// @returns A NativeWindowHandle structure containing native properties of the created window.
NativeWindowHandle createWebviewWindow(int width = 800, int height = 600);

/// Destroys a window and cleans up its resources.
/// @param handle A reference to the NativeWindowHandle structure of the window to destroy.
void destroyWebviewWindow(NativeWindowHandle& handle);


// -------- WebviewWindow class -------------------------------------------------------------------


class WebviewWindow
{

public:

    /// Native handle for the webview window. This is platform-specific and may vary depending on the OS.
    NativeWindowHandle m_windowHandle;

    WebviewWindow(int width = 800, int height = 600);
    WebviewWindow(std::string url, int width = 800, int height = 600);
    ~WebviewWindow();

    // -------- Window lifecycle --------

    /// Runs the webview window, entering the main event loop. This function blocks the thread where 
    /// it is called until the window is closed.
    void run();

    /// Terminates the webview window, closing it and exiting the main event loop.
    void close();

    /// Changes the URL of the webview window to the specified URL.
    /// @param url URL to navigate to. It can be a local file path or a remote URL.
    /// @note Usage example:
    /// ```cpp
    /// webviewWindow.navigate("https://example.com");
    /// webviewWindow.navigate("http://localhost:8080");
    /// webviewWindow.navigate("file:///home/user/path/to/local/file.html");
    /// ```
    void navigate(const std::string& url);

    /// Evaluates JavaScript code in the webview context. 
    /// The code is executed asynchronously.
    void eval(const std::string& jsCode);

    /// Sets JavaScript code to be executed every time a new document is loaded in the webview.
    void evalOnDocumentCreated(const std::string& jsCode);

    // -------- Events & Bindings --------

    /// Emits an event to the webview window.
    /// Events can be listened to via listen() in the webview.
    /// @param eventName The name of the event to emit.
    /// @param detail Optional JSON value containing additional data related to the event.
    void emit(const std::string& eventName, const Json::Value& detail = Json::nullValue);

    /// Binds a C++ function to a JavaScript function in the webview context.
    /// Bindings can be invoked via invoke() inside the webview.
    /// @tparam F The type of the function to bind. It must be a callable that returns either void or Json::Value. 
    /// Arguments can be none, a vector of Json::Value (args), or a reference to the WebviewWindow instance followed 
    /// by a vector of Json::Value (args).
    /// @param name The name of the binding, which will be used to call the function from JavaScript.
    /// @param fn The function to bind, which will be called when the binding is invoked from JavaScript.
    template <BindableCallback F>
    void bind(const std::string& name, F&& fn);

    // -------- Window Manipulation & Querying --------

    void setTitle(const std::string& title); /// Sets the title of the window (taskbar, mission control, etc.).
    void setResizable(bool resizable); /// Sets whether the window can be resized by the user.
    void setAlwaysOnTop(bool alwaysOnTop); /// Sets whether the window should always stay on top of other windows.
    void setSize(int width, int height); /// Resizes the window to the specified width and height.
    void setMinSize(int minWidth, int minHeight); /// Sets the minimum size of the window it can be resized to.
    void setMaxSize(int maxWidth, int maxHeight); /// Sets the maximum size of the window it can be resized to.
    void setPosition(int x, int y); /// Moves the window to the specified screen coordinates (x, y).
    void move(int deltaX, int deltaY); /// Moves the window by the specified delta (deltaX, deltaY).
    void maximize(); /// Maximizes the window
    void minimize(); /// Minimizes the window
    void restore(); /// Restores (unmaximizes) the window
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

    // -------- Style --------

    /// Sets the backdrop effect of the window.
    void setBackdrop(WindowBackdropEffect backdrop);

    /// Sets the frame style of the window (title bar and borders).
    /// This might affect how users interact with the window. For example, windows with no 
    /// title bar will not be draggable unless a drag area is implemented via HTML in the webview. 
    void setFrame(WindowFrameStyle frameStyle);

    // -------- Helpers --------

    /// Parses a JSON-formatted string originated from a webview binding request.
    /// @param req The JSON-formatted request string to parse, which is expected to be an array of arguments.
    /// @param outArgs A reference to a vector of strings where the extracted arguments will be stored.
    /// @returns true if parsing was successful; false otherwise.
    static bool parseWebviewReq(
        const std::string& req,
        JsonArgsVector& outArgs
    );

protected:
    
    /// Internal method to expose a C++ function to the webview context.
    /// The exposed method is callable via `window.webview[name]` and can 
    /// receive an infinite amount of arguments.
    void exposeFunctionToWebview(const std::string& name, std::function<Json::Value(const JsonArgsVector&)> fn);

private:

    /// Maps registered binding names to their corresponding C++ callback functions that
    /// will be executed when the binding is invoked from JavaScript.
    std::unordered_map<std::string, std::function<Json::Value(const JsonArgsVector&)>> m_bindings;

    /// Maximum and minimum size constraints for the window.
    WindowSizeConstraints m_sizeConstraints;

    /// Initializes a listener that will handle incoming binding requests from the webview.
    void setupBindingsListener();

    /// Subscribes to window events (resize, move, close...) and forwards them to the WebviewWindow instance.
    void subscribeToWindowEvents();

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
                           
                // (args) -> Json::Value
                if constexpr (std::is_invocable_r_v<Json::Value, F, const JsonArgsVector&>) 
                    return fn(args);
                             
                // () -> Json::Value
                if constexpr (std::is_invocable_r_v<Json::Value, F>) 
                    return fn();

                // (win, args) -> void
                if constexpr (std::is_invocable_r_v<void, F, WebviewWindow&, const JsonArgsVector&>) {
                    fn(win, args); 
                    return Json::Value::nullRef;
                }

                // (args) -> void
                if constexpr (std::is_invocable_r_v<void, F, const JsonArgsVector&>) {
                    fn(args);
                    return Json::Value::nullRef;
                }
            
                // () -> void
                if constexpr (std::is_invocable_r_v<void, F>) {
                    fn();
                    return Json::Value::nullRef;
                }
            };
    }

} // namespace

template <BindableCallback F>
void WebviewWindow::bind(const std::string& name, F&& fn) {
    auto callbackFn = makeUnifiedBindingCallback(std::forward<F>(fn));

    // A binding is a synchronous function call from JavaScript to C++.
    // Binding identifiers are namespaced with "binding:" to avoid conflicts with other identifiers.
    exposeFunctionToWebview(
        "binding:" + name,
        [this, callbackFn]
        (const JsonArgsVector& args) -> Json::Value
        {
            Json::Value ret = callbackFn(*this, args);
            std::ostringstream ss;
            ss << std::boolalpha << R"({"status":true,"value":)" << ret.toStyledString() << "}";
            return ss.str();
        }
    );
}