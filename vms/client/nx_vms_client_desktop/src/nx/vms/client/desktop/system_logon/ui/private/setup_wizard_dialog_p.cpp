// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "setup_wizard_dialog_p.h"

#include <QtGui/QDesktopServices>
#include <QtQuick/QQuickItem>

#include <client_core/client_core_module.h>
#include <network/system_helpers.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/desktop/common/widgets/webview_widget.h>
#include <ui/graphics/items/standard/graphics_web_view.h>
#include <watchers/cloud_status_watcher.h>

#include "../setup_wizard_dialog.h"

namespace nx::vms::client::desktop {

SetupWizardDialogPrivate::SetupWizardDialogPrivate(
    SetupWizardDialog* parent)
    :
    QObject(parent),
    q(parent),
    webViewWidget(new WebViewWidget(parent))
{
    QObject::connect(webViewWidget->controller(), &WebViewController::windowCloseRequested,
        q, &SetupWizardDialog::accept);

    webViewWidget->controller()->setRedirectLinksToDesktop(true);
    webViewWidget->controller()->setMenuNavigation(false);

    static const QString kExportedName("nativeClient");
    webViewWidget->controller()->registerObject(kExportedName, this);
}

void SetupWizardDialogPrivate::load(const QUrl& url)
{
    // Setup dialog JavaScript is going to close itself via window.close(),
    // so the url should be opened by JavaScript.
    // Otherwise the browser will deny it with the following error:
    //    Scripts may close only the windows that were opened by it.

    const QString script = QString("window.location.replace(\'%1\')").arg(url.toString());
    webViewWidget->controller()->runJavaScript(script);
}

void SetupWizardDialogPrivate::cancel()
{
    // Remove 'accept' connection.
    if (QQuickItem* webView = webViewWidget->rootObject())
        webView->disconnect(q);

    // Security fix to make sure we will never try to login further.
    loginInfo = {};

    q->reject();
}

void SetupWizardDialogPrivate::openUrlInBrowser(const QString &urlString)
{
    QUrl url(urlString);

    NX_INFO(this, lit("External URL requested: %1")
        .arg(urlString));

    if (!url.isValid())
    {
        NX_WARNING(this, lit("External URL is invalid: %1")
            .arg(urlString));
        return;
    }

    QDesktopServices::openUrl(url);
}

void SetupWizardDialogPrivate::connectUsingLocalAdmin(const QString& password, bool savePassword)
{
    loginInfo.localPassword = password;
    loginInfo.savePassword = savePassword;
}

} // namespace nx::vms::client::desktop
