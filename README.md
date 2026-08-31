# webview-cpp

_webview-cpp_ is a C++ library that provides a simple and lightweight way to create web-based user interfaces for desktop applications. It provides a multiplatform (Windows [WebView2] and macOS [WebKit]) library for managing webview windows.

Also, a [NPM package]("https://github.com/oscar30gt/webview-cpp-api") is available for apps built with webview-cpp, providing a simple interface to communicate between the webview and the C++ application.

## Installation

1. Clone the repository:

   ```bash
   git clone "https://github.com/oscar30gt/webview-cpp.git"
   ```

2. Generate a single header file for the library using the provided `amalgamate.py` script:

   ```bash
   python amalgamate.py --single-header
   ```

   - Or bundle it while keeping separate header and implementation files:

   ```bash
   python amalgamate.py
   ```

3. Include the generated header file in your C++ project.

> [!IMPORTANT]
> **Windows:** You will need to download the [WebView2](https://www.nuget.org/packages/microsoft.web.webview2) header files and place them in your project (inside a `lib/` directory).

## Example Project

You can compile a simple example project using the provided `main.example.cc` file. Compile it with the following command:

```bash
g++ main.example.cc -o main.exe -lole32 -lshell32 -luuid -lcomctl32 -ldwmapi -lgdi32 -std=c++20 # Windows
clang++ -x objective-c++ main.example.cc -o main -std=c++20 -framework WebKit -framework Cocoa  # macOS
```

## License

[MIT](LICENSE)