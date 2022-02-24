// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "web_engine_dialog.h"

#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QLineEdit>

#include <nx/utils/log/log.h>
#include <nx/vms/client/desktop/common/widgets/webview_widget.h>
#include <ui/workbench/workbench_context.h>

#include "../utils/debug_custom_actions.h"

namespace nx::vms::client::desktop {

WebEngineDialog::WebEngineDialog(QWidget* parent):
    base_type(parent, Qt::Window),
    m_webViewWidget(new WebViewWidget(this)),
    m_urlLineEdit(new QLineEdit(this))
{
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(QMargins());
    layout->addWidget(m_urlLineEdit);
    layout->addWidget(m_webViewWidget, 1);
    connect(m_urlLineEdit, &QLineEdit::returnPressed, this,
        [this]
        {
            m_webViewWidget->controller()->load(m_urlLineEdit->text());
        });

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
        &WebEngineDialog::reject);

    m_webViewWidget->controller()->setRedirectLinksToDesktop(true);
    m_webViewWidget->controller()->setMenuNavigation(false);

    static const QString kExportedName("nativeClient");
    m_webViewWidget->controller()->registerObject(kExportedName, this);
}

void WebEngineDialog::registerAction()
{
    registerDebugAction(
        "WebEngine",
        [](QnWorkbenchContext* context)
        {
            auto dialog = std::make_unique<WebEngineDialog>(context->mainWindowWidget());
            dialog->exec();
        });
}

void WebEngineDialog::handleJsonObject(const QString& json)
{
    NX_INFO(this, json);
}

} // namespace nx::vms::client::desktop
