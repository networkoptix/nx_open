#pragma once

#include <QtCore/QString>
#include <QtGui/QPalette>

class QGraphicsWidget;
class QWidget;

namespace NxUi {

enum class WebViewStyle
{
    common,
    eula,
    c2p,
};

QPalette createWebViewPalette(WebViewStyle style = WebViewStyle::common);
void setupWebViewStyle(QWidget* webView, WebViewStyle style = WebViewStyle::common);
void setupWebViewStyle(QGraphicsWidget* webView, WebViewStyle style = WebViewStyle::common);

/** Generates a CSS according to our customization style. */
QString generateCssStyle();

} // namespace NxUi
