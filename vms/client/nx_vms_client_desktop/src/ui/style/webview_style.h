#pragma once

#include <QtCore/QString>
#include <QtGui/QPalette>

class QWebView;
class QGraphicsWebView;

namespace NxUi {

enum class WebViewStyle
{
    common,
    eula,
    c2p,
};

QPalette createWebViewPalette(WebViewStyle style = WebViewStyle::common);
void setupWebViewStyle(QWebView* webView, WebViewStyle style = WebViewStyle::common);
void setupWebViewStyle(QGraphicsWebView* webView, WebViewStyle style = WebViewStyle::common);

/** Generates a CSS according to our customization style. */
QString generateCssStyle();

} // namespace NxUi
