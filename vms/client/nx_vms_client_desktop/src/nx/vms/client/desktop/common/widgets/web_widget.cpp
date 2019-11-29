#include "web_widget.h"

#include <QtWidgets/QLabel>
#include <QtWidgets/QProgressBar>
#include <QtWebEngineWidgets/QWebEngineHistory>

#include <ui/style/webview_style.h>

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

    auto progressBar = new QProgressBar(this);
    anchorWidgetToParent(progressBar, Qt::TopEdge | Qt::LeftEdge | Qt::RightEdge);
    progressBar->setStyleSheet(QString::fromUtf8(R"(
        QProgressBar {
            border-top: 0px;
            border-bottom: 1px;
            border-style: ourset;
        }
        )"));
    progressBar->setFixedHeight(3);
    progressBar->setTextVisible(false);
    progressBar->setRange(0, 0);

    connect(m_webEngineView, &QWebEngineView::loadStarted, progressBar, &QWidget::show);
    connect(m_webEngineView, &QWebEngineView::loadFinished, progressBar, &QWidget::hide);
}

WebEngineView* WebWidget::webEngineView() const
{
    return m_webEngineView;
}

void WebWidget::load(const QUrl& url)
{
    m_webEngineView->load(url);
}

} // namespace nx::vms::client::desktop
