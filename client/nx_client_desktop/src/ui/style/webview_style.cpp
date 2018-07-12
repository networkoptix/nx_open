#include "webview_style.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QStyleFactory>

#include <QtWebKitWidgets/QWebView>
#include <QtWebKitWidgets/QGraphicsWebView>

namespace NxUi {

namespace {

static const QString kStyleName = lit("fusion");

} // namespace

QPalette createWebViewPalette(WebViewStyle style)
{
    QPalette result = qApp->palette();

    switch (style)
    {
        case WebViewStyle::common:
        {
            const QColor light(Qt::white);
            const QColor dark(Qt::black);
            result.setColor(QPalette::Base, light);
            result.setColor(QPalette::Window, light);
            result.setColor(QPalette::Text, dark);
            result.setColor(QPalette::HighlightedText, light);
            result.setColor(QPalette::Button, light);
            result.setColor(QPalette::ButtonText, dark);
            break;
        }
        default:
            break;
    }


    return result;
}

void setupWebViewStyle(QWebView* webView, WebViewStyle style)
{
    webView->setStyle(QStyleFactory::create(kStyleName));
    webView->setPalette(createWebViewPalette(style));
}

void setupWebViewStyle(QGraphicsWebView* webView, WebViewStyle style)
{
    webView->setStyle(QStyleFactory::create(kStyleName));
    webView->setPalette(createWebViewPalette(style));
    //webView->setAutoFillBackground(true);
}

} // namespace NxUi
