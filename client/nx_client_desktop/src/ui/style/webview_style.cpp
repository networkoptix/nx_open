#include "webview_style.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QStyleFactory>

#include <QtWebKitWidgets/QWebView>
#include <QtWebKitWidgets/QGraphicsWebView>

namespace {

QPalette createWebViewPalette()
{
    QPalette result = qApp->palette();
    QColor base = Qt::white;
    result.setColor(QPalette::Base, base);
    result.setColor(QPalette::Window, base);
    return result;
}

}

namespace NxUi {

void setupWebViewStyle(QWebView* webView)
{
    webView->setStyle(QStyleFactory().create(lit("fusion")));
    webView->setPalette(createWebViewPalette());
    webView->setAutoFillBackground(true);
    webView->setBackgroundRole(QPalette::Base);
    webView->setForegroundRole(QPalette::Base);
}

void setupWebViewStyle(QGraphicsWebView* webView)
{
    webView->setStyle(QStyleFactory().create(lit("fusion")));
    webView->setPalette(createWebViewPalette());
    webView->setAutoFillBackground(true);
}

}
