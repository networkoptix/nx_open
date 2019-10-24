#include "web_widget.h"

#include <QtNetwork/QNetworkReply>
#include <QtWebKitWidgets/QWebView>
#include <QtWebKitWidgets/QWebFrame>
#include <QtWidgets/QLabel>

#include <ui/style/webview_style.h>

#include <nx/vms/client/desktop/common/widgets/busy_indicator.h>
#include <nx/vms/client/desktop/common/widgets/web_engine_view.h>
#include <nx/vms/client/desktop/common/utils/widget_anchor.h>
#include <nx/vms/client/desktop/ini.h>

#include <utils/common/event_processors.h>

namespace nx::vms::client::desktop {

WebWidget::WebWidget(QWidget* parent, bool useActionsForLinks):
    base_type(parent)
{
    QWidget* view = nullptr;
    if (ini().useWebEngine)
    {
        m_webEngineView = new WebEngineView(this);
        m_webEngineView->setUseActionsForLinks(true);
        view = m_webEngineView;
    }
    else
    {
        m_webView = new QWebView(this);
        view = m_webView;

        if (useActionsForLinks)
        {
            installEventHandler(m_webView, QEvent::ContextMenu, this,
            [this](QObject* watched, QEvent* event)
            {
                NX_ASSERT(watched == m_webView && event->type() == QEvent::ContextMenu);
                const auto frame = m_webView->page()->mainFrame();
                if (!frame)
                    return;

                const auto menuEvent = static_cast<QContextMenuEvent*>(event);
                const auto hitTest = frame->hitTestContent(menuEvent->pos());

                m_webView->setContextMenuPolicy(hitTest.linkUrl().isEmpty()
                    ? Qt::DefaultContextMenu
                    : Qt::ActionsContextMenu); //< Special context menu for links.
            });
        }
    }

    NxUi::setupWebViewStyle(view);
    anchorWidgetToParent(view);

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

    if (m_webView)
    {
        connect(m_webView, &QWebView::loadStarted, busyIndicator, &QWidget::show);
        connect(m_webView, &QWebView::loadFinished, busyIndicator, &QWidget::hide);
        connect(m_webView, &QWebView::loadStarted, errorLabel, &QWidget::hide);
        connect(m_webView, &QWebView::loadFinished, errorLabel, &QWidget::setHidden);
    }
    else
    {
        connect(m_webEngineView, &QWebEngineView::loadStarted, busyIndicator, &QWidget::show);
        connect(m_webEngineView, &QWebEngineView::loadFinished, busyIndicator, &QWidget::hide);
        connect(m_webEngineView, &QWebEngineView::loadStarted, errorLabel, &QWidget::hide);
        connect(m_webEngineView, &QWebEngineView::loadFinished, this,
            [this, errorLabel](bool ok)
            {
                static const QUrl kEmptyPage("about:blank");
                errorLabel->setVisible(m_webEngineView->url() == kEmptyPage);
                if (ok)
                    return;
                // Replicate QWebKit behavior.
                m_webEngineView->load(kEmptyPage);
            });
    }
}

QWebView* WebWidget::view() const
{
    return m_webView;
}

WebEngineView* WebWidget::webEngineView() const
{
    return m_webEngineView;
}

void WebWidget::load(const QUrl& url)
{
    if (m_webView)
        m_webView->load(url);
    else
        m_webEngineView->load(url);
}

void WebWidget::load(const QNetworkRequest& request)
{
    if (m_webView)
        m_webView->load(request);
    else
        m_webEngineView->load(request.url());
}

void WebWidget::reset()
{
    if (m_webView)
    {
        m_webView->triggerPageAction(QWebPage::Stop);
        m_webView->triggerPageAction(QWebPage::StopScheduledPageRefresh);
        m_webView->setContent({});
    }
    else
    {
        m_webEngineView->triggerPageAction(QWebEnginePage::Stop);
        //FIXME: There is no analog for StopScheduledPageRefresh
        m_webEngineView->setContent({});
    }

}

} // namespace nx::vms::client::desktop
