// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "help_dialog.h"

#include <QtWidgets/QBoxLayout>

#include <nx/utils/log/log.h>
#include <nx/vms/client/desktop/common/widgets/webview_widget.h>

namespace {

constexpr QSize kInitialDialogSize(1024, 768);

} // namespace

namespace nx::vms::client::desktop {

HelpDialog::HelpDialog(QWidget* parent):
    base_type(parent, Qt::Window),
    m_webViewWidget(new WebViewWidget(this))
{
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(QMargins());
    layout->addWidget(m_webViewWidget, 1);

    connect(
        m_webViewWidget->controller(),
        &WebViewController::windowCloseRequested,
        this,
        [this]
        {
            NX_INFO(this, "Window close requested");
        });

    connect(
        m_webViewWidget->controller(),
        &WebViewController::windowCloseRequested,
        this,
        &HelpDialog::reject);

    m_webViewWidget->controller()->setRedirectLinksToDesktop(true);
    m_webViewWidget->controller()->setMenuNavigation(false);

    resize(kInitialDialogSize);
}

void HelpDialog::setUrl(const QString& url)
{
    m_webViewWidget->controller()->load(url);
}

} // namespace nx::vms::client::desktop
