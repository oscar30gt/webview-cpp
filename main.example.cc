// Windows: g++ main.example.cc -o main.exe -lole32 -lshell32 -luuid -lcomctl32 -ldwmapi -lgdi32 -std=c++20
// macOS:   clang++ main.example.cc -o main -std=c++20 -x objective-c++ -framework WebKit -framework Cocoa

#include "dist/webview-cpp.h"
#include <iostream>

int main() {

    std::string url;
    std::cout << "URL: ";
    std::cin >> url;

    WebviewWindow win(800, 600);
    win.navigate(url);
    win.setBackdrop(WindowBackdropEffect::Vibrant);
    win.setFrame(WindowFrameStyle::NoTitlebar);
    win.run();

    return 0;
}
