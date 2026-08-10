#include "dist/webview-cpp.h"

int main() {
    WebviewWindow win("http://localhost:5173/", 800, 600);
    win.setTitle("My Webview Window");
    win.run();

    return 0;
}