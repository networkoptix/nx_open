// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_actions_handler.h"

#include <QtGui/QDesktopServices>
#include <QtGui/QAction>

#include <client/client_globals.h>
#include <core/resource/user_resource.h>
#include <nx/branding.h>
#include <nx/vms/api/data/module_information.h>
#include <nx/vms/client/core/common/utils/cloud_url_helper.h>
#include <nx/vms/client/core/network/cloud_auth_data.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/state/shared_memory_manager.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <ui/workbench/workbench_context.h>
#include <utils/connection_diagnostics_helper.h>

#include "../ui/oauth_login_dialog.h"

namespace nx::vms::client::desktop {

CloudActionsHandler::CloudActionsHandler(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    connect(action(ui::action::LoginToCloud), &QAction::triggered, this,
        &CloudActionsHandler::at_loginToCloudAction_triggered);
    connect(action(ui::action::LogoutFromCloud), &QAction::triggered, this,
        &CloudActionsHandler::at_logoutFromCloudAction_triggered);

    auto openUrl =
        [](QUrl url)
        {
            return [url](){ QDesktopServices::openUrl(url); };
        };

    core::CloudUrlHelper urlHelper(
        nx::vms::utils::SystemUri::ReferralSource::DesktopClient,
        nx::vms::utils::SystemUri::ReferralContext::CloudMenu);

    connect(action(ui::action::OpenCloudMainUrl), &QAction::triggered, this,
        openUrl(urlHelper.mainUrl()));

    connect(action(ui::action::OpenCloudViewSystemUrl), &QAction::triggered, this,
        openUrl(urlHelper.viewSystemUrl()));

    connect(action(ui::action::OpenCloudManagementUrl), &QAction::triggered, this,
        openUrl(urlHelper.accountManagementUrl()));

    connect(action(ui::action::OpenCloudRegisterUrl), &QAction::triggered, this,
        openUrl(urlHelper.createAccountUrl()));

    connect(action(ui::action::OpenCloudAccountSecurityUrl), &QAction::triggered, this,
        openUrl(urlHelper.accountSecurityUrl()));

    // Forcing logging out if found 'logged out' status.
    // Seems like it can cause double disconnect.
    auto watcher = qnCloudStatusWatcher;
    connect(watcher, &core::CloudStatusWatcher::forcedLogout,
        this, &CloudActionsHandler::at_forcedLogout);
    connect(watcher, &core::CloudStatusWatcher::loggedOutWithError, this,
        &CloudActionsHandler::at_logout);

    connect(
        appContext()->sharedMemoryManager(),
        &SharedMemoryManager::clientCommandRequested,
        this,
        [this](SharedMemoryData::Command command, const QByteArray& /*data*/)
        {
            if (command == SharedMemoryData::Command::logoutFromCloud)
            {
                logoutFromCloud();

                if (context()->user() && context()->user()->isCloud())
                    menu()->trigger(ui::action::DisconnectAction, {Qn::ForceRole, true});
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
        core::OauthClientType::loginCloud,
        /*sessionAware*/ false);

    if (!authData.empty())
        qnCloudStatusWatcher->setAuthData(authData, /*forced*/true);
}

void CloudActionsHandler::at_logoutFromCloudAction_triggered()
{
    logoutFromCloud();
    appContext()->sharedMemoryManager()->requestLogoutFromCloud();
}

void CloudActionsHandler::at_logout()
{
    QnConnectionDiagnosticsHelper::showConnectionErrorMessage(
        context(),
        nx::vms::client::core::RemoteConnectionErrorCode::cloudSessionExpired,
        /*moduleInformation*/ {},
        appContext()->version());
}

void CloudActionsHandler::at_forcedLogout()
{
    menu()->trigger(ui::action::LogoutFromCloud);

    QnConnectionDiagnosticsHelper::showConnectionErrorMessage(
        context(),
        nx::vms::client::core::RemoteConnectionErrorCode::cloudSessionExpired,
        /*moduleInformation*/ {},
        appContext()->version());
}

} // namespace nx::vms::client::desktop
