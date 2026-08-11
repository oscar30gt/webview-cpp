// g++ main.example.cc -o main.exe -lole32 -lshell32 -luuid -lcomctl32 -ldwmapi -lgdi32 -std=c++20

#include "dist/webview-cpp.h"

int main() {
    WebviewWindow win(800, 600);
    win.navigate("http://localhost:5173/");
    win.setBackdrop(WindowBackdropEffect::Vibrant);
    win.setFrame(WindowFrameStyle::NoTitlebar);
    win.run();

    return 0;
}