#ifdef __APPLE__

#import <WebKit/WebKit.h>
#include <string>
#include <sstream>
#include <unordered_map>
#include <functional>

#include "../webviewWindow.h"

// -------- Script Message Handler ----------------------------------------------------------------

@interface _WebviewScriptHandler : NSObject <WKScriptMessageHandler>
@property (nonatomic, assign) WebviewWindow* cppWindow;
@property (nonatomic, assign) std::unordered_map<std::string, std::function<Json::Value(const JsonArgsVector&)>>* bindings;
@end

@implementation _WebviewScriptHandler

- (void)userContentController:(WKUserContentController*)userContentController
      didReceiveScriptMessage:(WKScriptMessage*)message {
    if (!_cppWindow || !_bindings) return;

    NSString* jsonString = nil;
    if ([message.body isKindOfClass:[NSString class]]) {
        jsonString = (NSString*)message.body;
    } else if ([message.body isKindOfClass:[NSArray class]] || [message.body isKindOfClass:[NSDictionary class]]) {
        NSData* jsonData = [NSJSONSerialization dataWithJSONObject:message.body options:0 error:nil];
        if (jsonData) {
            jsonString = [[NSString alloc] initWithData:jsonData encoding:NSUTF8StringEncoding];
        }
    }

    if (!jsonString) return;

    std::string msg([jsonString UTF8String]);
    JsonArgsVector argsJson;
    if (!WebviewWindow::parseWebviewReq(msg, argsJson) || argsJson.size() < 2) {
        return;
    }

    std::string name = argsJson.back().asString();
    argsJson.pop_back();
    int seq = argsJson.back().asInt();
    argsJson.pop_back();

    auto it = _bindings->find(name);
    if (it != _bindings->end()) {
        Json::Value response;
        response["ack"] = seq;

        try {
            Json::Value result = it->second(argsJson);
            response["status"] = "success";
            response["result"] = result;
        } catch (const std::exception& e) {
            response["status"] = "error";
            response["error"] = e.what();
        }

        _cppWindow->emit("bindingResult", response);
    }
}

@end

// -------- WebviewWindow Bindings Implementation -------------------------------------------------

void WebviewWindow::setupBindingsListener() {
    WKWebView* wv = (__bridge WKWebView*)m_windowHandle.wkWebView;
    if (!wv) return;

    _WebviewScriptHandler* handler = [[_WebviewScriptHandler alloc] init];
    handler.cppWindow = this;
    handler.bindings = &m_bindings;

    [wv.configuration.userContentController addScriptMessageHandler:handler name:@"webviewBridge"];
    m_windowHandle.scriptDelegate = (__bridge_retained void*)handler;
}

void WebviewWindow::exposeFunctionToWebview(const std::string& name, std::function<Json::Value(const JsonArgsVector&)> fn) {
    std::string functionCreationString =
        "if (!window.webview) window.webview = {};"
        "window.webview['" + name + "'] = (...args) => {"
            "args.push('" + name + "');"
            "window.webkit.messageHandlers.webviewBridge.postMessage(JSON.stringify(args));"
        "};";

    evalOnDocumentCreated(functionCreationString);
    m_bindings[name] = fn;
}

#endif // __APPLE__
