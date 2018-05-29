#include "webview_style.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QStyleFactory>

#include <QtWebKitWidgets/QWebView>
#include <QtWebKitWidgets/QGraphicsWebView>

namespace NxUi {

namespace {

static const QString kStyleName = lit("fusion");

QPalette createWebViewPalette(WebViewStyle style)
{
    QPalette result = qApp->palette();

    switch (style)
    {
        case WebViewStyle::common:
        {
            const QColor base = Qt::white;
            result.setColor(QPalette::Base, base);
            result.setColor(QPalette::Window, base);
            break;
        }
        default:
            break;
    }


    return result;
}

} // namespace

void setupWebViewStyle(QWebView* webView, WebViewStyle style)
{
    webView->setStyle(QStyleFactory::create(kStyleName));
    webView->setPalette(createWebViewPalette(style));
    if (style == WebViewStyle::common)
    {
        webView->setAutoFillBackground(true);
        webView->setBackgroundRole(QPalette::Base);
        webView->setForegroundRole(QPalette::Base);
    }
}

void setupWebViewStyle(QGraphicsWebView* webView, WebViewStyle style)
{
    webView->setStyle(QStyleFactory::create(kStyleName));
    webView->setPalette(createWebViewPalette(style));
    webView->setAutoFillBackground(true);
}

} // namespace NxUi
