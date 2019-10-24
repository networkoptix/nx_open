#include "webview_style.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QStyleFactory>
#include <QtWidgets/QWidget>
#include <QtWidgets/QGraphicsWidget>

#include <ui/style/nx_style.h>

#include <nx/vms/client/desktop/ui/common/color_theme.h>

using namespace nx::vms::client::desktop;

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
        color: {windowText};
        font-family: 'Roboto-Regular', 'Roboto';
        font-weight: 600;
        font-size: 13px;
        line-height: 16px;
    }
    body {
        padding-left: 0px;
        margin: 0px;
        overscroll-behavior: none;
        background-color: {window};
    }
    p {
        padding-left: 0px;
    }
    a {
        color: {link};
        font-size: 13px;
    }
    a:hover {
        color: {highlight};
    }
    ::-webkit-scrollbar {
        width: 8px;
    }
    ::-webkit-scrollbar-track {
        background: {scrollbar-track};
    }
    ::-webkit-scrollbar-thumb {
        background: {scrollbar-thumb};
    }
    ::-webkit-scrollbar-thumb:hover {
        background: {scrollbar-thumb-hover};
    }
    )css");

    const auto palette = qApp->palette();

    return QString(styleBase)
        .replace("{windowText}", palette.color(QPalette::WindowText).name())
        .replace("{window}", palette.color(QPalette::Window).name())
        .replace("{link}", palette.color(QPalette::Link).name())
        .replace("{highlight}", palette.color(QPalette::Highlight).name())
        .replace("{scrollbar-track}", colorTheme()->color("dark9").name())
        .replace("{scrollbar-thumb}", colorTheme()->color("dark13").name())
        .replace("{scrollbar-thumb-hover}", colorTheme()->color("dark15").name())
        .simplified();
}

} // namespace NxUi
