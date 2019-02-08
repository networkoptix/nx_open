#include "web_widget.h"

#include <QtNetwork/QNetworkReply>
#include <QtWebKitWidgets/QWebView>
#include <QtWidgets/QLabel>

#include <ui/style/webview_style.h>

#include <nx/vms/client/desktop/common/widgets/busy_indicator.h>
#include <nx/vms/client/desktop/common/utils/widget_anchor.h>

namespace nx::vms::client::desktop {

WebWidget::WebWidget(QWidget* parent):
    base_type(parent),
    m_webView(new QWebView(this))
{
    NxUi::setupWebViewStyle(m_webView);
    anchorWidgetToParent(m_webView);

    static constexpr int kDotRadius = 8;
    auto busyIndicator = new BusyIndicatorWidget(parent);
    busyIndicator->setHidden(true);
    busyIndicator->dots()->setDotRadius(kDotRadius);
    busyIndicator->dots()->setDotSpacing(kDotRadius * 2);
    anchorWidgetToParent(busyIndicator);

    auto errorLabel = new QLabel(parent);
    errorLabel->setText(lit("<h1>%1</h1>").arg(tr("Failed to load page")));
    errorLabel->setAlignment(Qt::AlignCenter);
    errorLabel->setForegroundRole(QPalette::WindowText);
    anchorWidgetToParent(errorLabel);

    connect(m_webView, &QWebView::loadStarted, busyIndicator, &QWidget::show);
    connect(m_webView, &QWebView::loadFinished, busyIndicator, &QWidget::hide);
    connect(m_webView, &QWebView::loadStarted, errorLabel, &QWidget::hide);
    connect(m_webView, &QWebView::loadFinished, errorLabel, &QWidget::setHidden);
}

QWebView* WebWidget::view() const
{
    return m_webView;
}

void WebWidget::load(const QUrl& url)
{
    m_webView->load(url);
}

void WebWidget::load(const QNetworkRequest& request)
{
    m_webView->load(request);
}

void WebWidget::reset()
{
    m_webView->triggerPageAction(QWebPage::Stop);
    m_webView->triggerPageAction(QWebPage::StopScheduledPageRefresh);
    m_webView->setContent({});
}

} // namespace nx::vms::client::desktop
