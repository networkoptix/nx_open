#include "webview_style.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QStyleFactory>
#include <QtWebKitWidgets/QWebView>
#include <QtWebKitWidgets/QGraphicsWebView>

#include <ui/style/nx_style.h>

namespace NxUi {

namespace {

static const QString kStyleName = "fusion";

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

QString generateCssStyle()
{
    const auto styleBase = QString::fromLatin1(R"css(
    * {
        color: %1;
        font-family: 'Roboto-Regular', 'Roboto';
        font-weight: 400;
        font-size: 13px;
        line-height: 16px;
    }
    body {
        padding-left: 0px;
        margin: 0px;
    }
    p {
        padding-left: 0px;
    }
    a {
        color: %2;
        font-size: 13px;
    }
    a:hover {
        color: %3;
    })css");

    const auto palette = qApp->palette();
    const auto windowText = palette.color(QPalette::WindowText).name();
    const auto highlight = palette.color(QPalette::Highlight).name();
    const auto link = palette.color(QPalette::Link).name();
    return styleBase.arg(windowText, link, highlight);
}

} // namespace NxUi
