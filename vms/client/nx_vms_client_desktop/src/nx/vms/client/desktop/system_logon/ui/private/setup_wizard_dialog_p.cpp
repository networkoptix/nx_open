// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "setup_wizard_dialog_p.h"

#include <QtGui/QDesktopServices>
#include <QtQuick/QQuickItem>

#include <client_core/client_core_module.h>
#include <network/system_helpers.h>
#include <nx/reflect/instrument.h>
#include <nx/reflect/json.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/widgets/webview_widget.h>
#include <ui/graphics/items/standard/graphics_web_view.h>

#include "../setup_wizard_dialog.h"

namespace nx::vms::client::desktop {

NX_REFLECTION_INSTRUMENT(SetupWizardDialogPrivate::LoginInfo,
    (localLogin)(localPassword)(savePassword));

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

    static const QString kExportedName("setupDialog");
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

QString SetupWizardDialogPrivate::getCredentials() const
{
    return QString::fromStdString(nx::reflect::json::serialize(loginInfo));
}

void SetupWizardDialogPrivate::updateCredentials(
    const QString& login,
    const QString& password,
    bool /*alwaysFalse*/, //< Waiting for webadmin fix.
    bool savePassword)
{
    loginInfo.localLogin = login;
    loginInfo.localPassword = password;
    loginInfo.savePassword = savePassword;
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

QString SetupWizardDialogPrivate::refreshToken() const
{
    if (auto watcher = qnCloudStatusWatcher; NX_ASSERT(watcher))
        return QString::fromStdString(watcher->remoteConnectionCredentials().authToken.value);

    return QString();
}

void SetupWizardDialogPrivate::connectUsingLocalAdmin(const QString& password)
{
    loginInfo.localLogin = helpers::kFactorySystemUser;
    loginInfo.localPassword = password;
    loginInfo.savePassword = false;
}

void SetupWizardDialogPrivate::connectUsingCloud()
{
    // TODO: #GDM To be implemented.
}

} // namespace nx::vms::client::desktop
