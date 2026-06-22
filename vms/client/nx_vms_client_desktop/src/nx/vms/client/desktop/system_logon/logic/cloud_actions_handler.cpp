// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_actions_handler.h"

#include <QtCore/QScopedValueRollback>
#include <QtCore/QTimer>
#include <QtCore/QUrl>
#include <QtGui/QDesktopServices>

#include <client/client_globals.h>
#include <core/resource/user_resource.h>
#include <nx/branding.h>
#include <nx/utils/log/log.h>
#include <nx/vms/api/data/module_information.h>
#include <nx/vms/client/core/common/utils/cloud_url_helper.h>
#include <nx/vms/client/core/network/cloud_auth_data.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/widgets/webview_widget.h>
#include <nx/vms/client/desktop/menu/action.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/state/shared_memory_manager.h>
#include <nx/vms/client/desktop/system_context.h>
#include <ui/workbench/workbench_context.h>
#include <utils/connection_diagnostics_helper.h>

#include "../ui/oauth_login_dialog.h"

namespace nx::vms::client::desktop {

namespace {

// Navigates an invisible in-app browser to the IdP (Keycloak) logout URL to terminate IdP session.
void terminateIdpSession(QWidget* mainWindow, const QUrl& logoutUrl)
{
    // The logout may bounce through an upstream IdP (e.g. Keycloak -> Azure AD via SAML) before
    // returning to the cloud portal. Keep the browser alive until isSsoLogoutComplete() sees that
    // return (or the timeout fires) - closing earlier aborts the chain, leaving the session alive.
    auto* web = new WebViewWidget(mainWindow);

    // Follow the IdP redirect chain in-page instead of handing links to the OS browser.
    web->controller()->setRedirectLinksToDesktop(false);
    web->controller()->setMenuNavigation(false);
    web->controller()->setLoadIcon(false);

    // WA_DontShowOnScreen renders the widget without ever putting a window on screen.
    web->setAttribute(Qt::WA_DontShowOnScreen);
    web->resize(1, 1);
    web->show();

    const auto finished = std::make_shared<bool>(false);
    const auto cleanup =
        [web, finished]()
        {
            if (*finished)
                return;
            *finished = true;
            web->deleteLater();
        };

    const auto checkUrl =
        [web, cleanup]()
        {
            if (qnCloudStatusWatcher->isSsoLogoutComplete(web->controller()->url()))
                cleanup();
        };

    QObject::connect(web->controller(), &WebViewController::urlChanged, web, checkUrl);
    QObject::connect(web->controller(), &WebViewController::loadFinished, web,
        [checkUrl](bool /*ok*/) { checkUrl(); });

    // Close after the whole logout chain has had time to complete, even when the provider ends on
    // its own sign-out page and never redirects back.
    auto* timeout = new QTimer(web);
    timeout->setSingleShot(true);
    QObject::connect(timeout, &QTimer::timeout, web, [cleanup]() { cleanup(); });
    timeout->start(core::CloudStatusWatcher::kSsoLogoutTimeout);

    web->controller()->load(logoutUrl);
}

} // namespace

CloudActionsHandler::CloudActionsHandler(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    connect(action(menu::LoginToCloud), &QAction::triggered, this,
        &CloudActionsHandler::at_loginToCloudAction_triggered);
    connect(action(menu::LogoutFromCloud), &QAction::triggered, this,
        &CloudActionsHandler::at_logoutFromCloudAction_triggered);

    auto openUrl =
        [](QUrl url)
        {
            return [url](){ QDesktopServices::openUrl(url); };
        };

    core::CloudUrlHelper urlHelper(
        nx::vms::utils::SystemUri::ReferralSource::DesktopClient,
        nx::vms::utils::SystemUri::ReferralContext::CloudMenu);

    connect(action(menu::OpenCloudMainUrl), &QAction::triggered, this,
        openUrl(urlHelper.mainUrl()));

    connect(action(menu::OpenCloudViewSystemUrl), &QAction::triggered, this,
        openUrl(urlHelper.viewSystemUrl(system())));

    connect(action(menu::OpenCloudManagementUrl), &QAction::triggered, this,
        openUrl(urlHelper.accountManagementUrl()));

    connect(action(menu::OpenCloudRegisterUrl), &QAction::triggered, this,
        openUrl(urlHelper.createAccountUrl()));

    connect(action(menu::OpenCloudAccountSecurityUrl), &QAction::triggered, this,
        openUrl(urlHelper.accountSecurityUrl()));

    // Forcing logging out if found 'logged out' status.
    // Seems like it can cause double disconnect.
    auto watcher = qnCloudStatusWatcher;
    connect(watcher, &core::CloudStatusWatcher::forcedLogout,
        this, &CloudActionsHandler::at_forcedLogout);
    connect(watcher, &core::CloudStatusWatcher::loggedOutWithError, this,
        &CloudActionsHandler::at_logout);

    // Terminate the SSO provider session in the in-app browser once its logout URL is ready.
    connect(watcher, &core::CloudStatusWatcher::ssoLogoutUrlReady, this,
        [this](const QUrl& logoutUrl)
        {
            if (auto* mainWindow = mainWindowWidget())
                terminateIdpSession(mainWindow, logoutUrl);
        });

    connect(
        appContext()->sharedMemoryManager(),
        &SharedMemoryManager::clientCommandRequested,
        this,
        [this](SharedMemoryData::Command command, const QByteArray& /*data*/)
        {
            if (command == SharedMemoryData::Command::logoutFromCloud)
            {
                // Local-only logout: the instance that started the logout drives the SSO browser
                // termination; other instances just clear their own auth.
                logoutFromCloud();

                if (context()->user() && context()->user()->isCloud())
                    menu()->trigger(menu::DisconnectAction, {Qn::ForceRole, true});
            }
        });
}

CloudActionsHandler::~CloudActionsHandler()
{
}

void CloudActionsHandler::logoutFromCloud()
{
    qnCloudStatusWatcher->resetAuthData();
}

void CloudActionsHandler::at_loginToCloudAction_triggered()
{
    auto authData = OauthLoginDialog::login(
        mainWindowWidget(),
        tr("Login to %1", "%1 is the cloud name (like Nx Cloud)")
            .arg(nx::branding::cloudName()),
        OauthLoginDialog::LoginParams{.clientType = core::OauthClientType::loginCloud});

    const auto actionParameters = menu()->currentParameters(sender());
    const auto authMode = actionParameters.argument(Qn::ForceRole).toBool()
        ? core::CloudStatusWatcher::AuthMode::forced
        : core::CloudStatusWatcher::AuthMode::login;

    if (authData)
        qnCloudStatusWatcher->setAuthData(*authData, authMode);
    else
        NX_WARNING(this, "Cloud login failed due to %1", authData.error());
}

void CloudActionsHandler::at_logoutFromCloudAction_triggered()
{
    // Forced logout only clears local auth; a real user logout also ends the SSO session.
    if (m_forcedLogout)
        logoutFromCloud();
    else
        qnCloudStatusWatcher->logoutWithSsoSessionTermination();

    appContext()->sharedMemoryManager()->requestLogoutFromCloud();
}

void CloudActionsHandler::at_logout()
{
    QnConnectionDiagnosticsHelper::showConnectionErrorMessage(
        windowContext(),
        nx::vms::client::core::RemoteConnectionErrorCode::cloudSessionExpired,
        /*moduleInformation*/ {},
        appContext()->version());
}

void CloudActionsHandler::at_forcedLogout()
{
    // Preserve the original forced-logout behavior (including the cloud-system disconnect performed
    // by ConnectActionsHandler), but suppress the SSO termination: the cloud session is already
    // invalid server-side, so there is no live provider session to terminate via the browser.
    {
        QScopedValueRollback<bool> guard(m_forcedLogout, true);
        menu()->trigger(menu::LogoutFromCloud);
    }

    QnConnectionDiagnosticsHelper::showConnectionErrorMessage(
        windowContext(),
        nx::vms::client::core::RemoteConnectionErrorCode::cloudSessionExpired,
        /*moduleInformation*/ {},
        appContext()->version());
}

} // namespace nx::vms::client::desktop
