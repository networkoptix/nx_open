#include "webview_style.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QStyleFactory>
#include <QWidget>
#include <QGraphicsWidget>

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

void setupWebViewStyle(QWidget* webView, WebViewStyle style)
{
    webView->setStyle(QStyleFactory::create(kStyleName));
    webView->setPalette(createWebViewPalette(style));
}

void setupWebViewStyle(QGraphicsWidget* webView, WebViewStyle style)
{
    webView->setStyle(QStyleFactory::create(kStyleName));
    webView->setPalette(createWebViewPalette(style));
    //webView->setAutoFillBackground(true);
}

QString generateCssStyle()
{
    const auto styleBase = QString::fromLatin1(R"css(
    @font-face {
        font-family: 'Roboto';
        src: url('qrc:/fonts/Roboto-Regular.ttf') format('truetype');
        font-style: normal;
    }
    * {
        color: %1;
        font-family: 'Roboto-Regular', 'Roboto';
        font-weight: 600;
        font-size: 13px;
        line-height: 16px;
    }
    body {
        padding-left: 0px;
        margin: 0px;
        overscroll-behavior: none;
        background-color: %2;
    }
    p {
        padding-left: 0px;
    }
    a {
        color: %3;
        font-size: 13px;
    }
    a:hover {
        color: %4;
    })css");

    const auto palette = qApp->palette();
    const auto windowText = palette.color(QPalette::WindowText).name();
    const auto window = palette.color(QPalette::Window).name();
    const auto highlight = palette.color(QPalette::Highlight).name();
    const auto link = palette.color(QPalette::Link).name();
    return styleBase.arg(windowText, window, link, highlight);
}

} // namespace NxUi
