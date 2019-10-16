#include "web_widget.h"

#include <QtNetwork/QNetworkReply>
#include <QtWebKitWidgets/QWebView>
#include <QWebFrame>
#include <QtWidgets/QLabel>
#include <QWebEngineView>
#include <QWebEngineContextMenuData>
#include <QMenu>

#include <ui/style/webview_style.h>

#include <nx/vms/client/desktop/common/widgets/busy_indicator.h>
#include <nx/vms/client/desktop/common/utils/widget_anchor.h>
#include <nx/vms/client/desktop/ini.h>

#include <utils/common/event_processors.h>

namespace {

class WebEngineView: public QWebEngineView
{
    using base_type = QWebEngineView;

public:
    WebEngineView(QWidget* parent, bool useActionsForLinks)
        : base_type(parent)
        , m_useActionsForLinks(useActionsForLinks)
    {}

protected:
    virtual void contextMenuEvent(QContextMenuEvent* event) override
    {
        if (m_useActionsForLinks)
        {
            const QWebEngineContextMenuData data = page()->contextMenuData();

            if (!data.linkUrl().isEmpty())
            {
                QMenu* menu = new QMenu(this);
                menu->addActions(actions());
                menu->popup(event->globalPos());
                return;
            }
        }

        base_type::contextMenuEvent(event);
    }
private:
    bool m_useActionsForLinks;
};

} // namespace

namespace nx::vms::client::desktop {

WebWidget::WebWidget(QWidget* parent, bool useActionsForLinks):
    base_type(parent),
    m_webView(nullptr),
    m_webEngineView(nullptr)
{
    QWidget* view = nullptr;
    if (ini().useWebEngine)
    {
        m_webEngineView = new WebEngineView(this, useActionsForLinks);
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
        connect(m_webEngineView, &QWebEngineView::loadFinished, errorLabel, &QWidget::setHidden);
    }
}

QWebView* WebWidget::view() const
{
    return m_webView;
}

QWebEngineView* WebWidget::webEngineView() const
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
