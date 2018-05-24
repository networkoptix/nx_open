#pragma once

class QWebView;
class QGraphicsWebView;

namespace NxUi {

enum class WebViewStyle
{
    common,
    eula
};

void setupWebViewStyle(QWebView* webView, WebViewStyle style = WebViewStyle::common);
void setupWebViewStyle(QGraphicsWebView* webView, WebViewStyle style = WebViewStyle::common);

} // namespace NxUi
