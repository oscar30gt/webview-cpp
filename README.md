# webview-cpp

webview-cpp is a C++ library that provides a simple and lightweight way to create web-based user interfaces for desktop applications. It is built on top of (webview/webview)[https://github.com/webview/webview], providing a more high-level interface for developers to create webview windows and interact with them.

Also, a [NPM package]() is available for apps built with webview-cpp, providing a simple interface to communicate between the webview and the C++ application.

## Installation

1. Clone the repository:

   ```bash
   git clone "https://github.com/oscar30gt/webview-cpp.git"
   ```

2. Build the single header file using python:

   ```bash
   python amalgamate.py
   ```

> This step is optional, but it is recommended to reduce the number of files in your project.

3. Use CMake to build the test project:

   ```bash
   cmake -B build
   cmake --build build
   ```

> [!NOTE]
> **WINDOWS:** As webview-cpp uses WebView2, you will need to download the [WebView2](https://www.nuget.org/packages/microsoft.web.webview2) header files and place them on the same directory as the amalgamated header file.
> If you opt not to use the amalgamated header file, place them on the lib/ directory.
