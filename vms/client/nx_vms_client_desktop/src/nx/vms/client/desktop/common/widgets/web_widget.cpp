#include "web_widget.h"

#include <QtWidgets/QLabel>

#include <ui/style/webview_style.h>

#include <nx/vms/client/desktop/common/widgets/busy_indicator.h>
#include <nx/vms/client/desktop/common/widgets/web_engine_view.h>
#include <nx/vms/client/desktop/common/utils/widget_anchor.h>
#include <nx/vms/client/desktop/ini.h>

#include <utils/common/event_processors.h>

namespace nx::vms::client::desktop {

WebWidget::WebWidget(QWidget* parent):
    base_type(parent)
{
    m_webEngineView = new WebEngineView(this);

    NxUi::setupWebViewStyle(m_webEngineView);
    anchorWidgetToParent(m_webEngineView);

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

WebEngineView* WebWidget::webEngineView() const
{
    return m_webEngineView;
}

void WebWidget::load(const QUrl& url)
{
    m_webEngineView->load(url);
}

void WebWidget::reset()
{
    m_webEngineView->triggerPageAction(QWebEnginePage::Stop);
    //FIXME: There is no analog for StopScheduledPageRefresh
    m_webEngineView->setContent({});
}

} // namespace nx::vms::client::desktop
