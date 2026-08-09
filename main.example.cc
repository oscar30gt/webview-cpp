#include "dist/webview-cpp.h"

int main() {
    WebviewWindow win("https://google.com", 800, 600);
    win.setTitle("My Webview Window");
    win.run();

    return 0;
}