// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "web_widget.h"

#include <QtWidgets/QLabel>
#include <QtWidgets/QProgressBar>

#include <nx/vms/client/desktop/style/webview_style.h>

#include <nx/vms/client/desktop/common/widgets/webview_widget.h>
#include <nx/vms/client/desktop/common/utils/widget_anchor.h>

namespace nx::vms::client::desktop {

WebWidget::WebWidget(QWidget* parent):
    base_type(parent)
{
    m_webView = new WebViewWidget(this);

    anchorWidgetToParent(m_webView);

    auto progressBar = new QProgressBar(this);
    anchorWidgetToParent(progressBar, Qt::TopEdge | Qt::LeftEdge | Qt::RightEdge);
    progressBar->setStyleSheet(QString::fromUtf8(R"(
        QProgressBar {
            border-top: 0px;
            border-bottom: 1px;
            border-style: outset;
        }
        )"));
    progressBar->setFixedHeight(3);
    progressBar->setTextVisible(false);
    progressBar->setRange(0, 0);

    connect(m_webView->controller(), &WebViewController::loadStarted, progressBar, &QWidget::show);
    connect(m_webView->controller(), &WebViewController::loadFinished, progressBar, &QWidget::hide);
}

WebViewController* WebWidget::controller() const
{
    return m_webView->controller();
}

} // namespace nx::vms::client::desktop
