#include "workbench_cloud_handler.h"

#include <QtGui/QDesktopServices>

#include <QtWidgets/QAction>

#include <api/app_server_connection.h>

#include <client/desktop_client_message_processor.h>
#include <client_core/client_core_settings.h>

#include <core/resource/user_resource.h>

#include <common/common_module.h>
#include <client/client_settings.h>

#include <helpers/cloud_url_helper.h>
#include <helpers/system_helpers.h>

#include <nx/vms/client/core/settings/client_core_settings.h>

#include <nx/vms/client/desktop/ui/actions/actions.h>
#include <nx/vms/client/desktop/ui/actions/action_parameters.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <ui/dialogs/cloud/login_to_cloud_dialog.h>
#include <ui/workbench/workbench_context.h>

#include <watchers/cloud_status_watcher.h>

using namespace nx::vms::client::desktop::ui;

QnWorkbenchCloudHandler::QnWorkbenchCloudHandler(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_cloudUrlHelper(new QnCloudUrlHelper(
        nx::vms::utils::SystemUri::ReferralSource::DesktopClient,
        nx::vms::utils::SystemUri::ReferralContext::CloudMenu,
        this))
{
    connect(action(action::LoginToCloud), &QAction::triggered, this,
        &QnWorkbenchCloudHandler::at_loginToCloudAction_triggered);
    connect(action(action::LogoutFromCloud), &QAction::triggered, this,
        &QnWorkbenchCloudHandler::at_logoutFromCloudAction_triggered);
    connect(action(action::OpenCloudMainUrl), &QAction::triggered, this,
        &QnWorkbenchCloudHandler::at_openCloudMainUrlAction_triggered);
    connect(action(action::OpenCloudViewSystemUrl), &QAction::triggered, this,
        [this]{QDesktopServices::openUrl(m_cloudUrlHelper->viewSystemUrl());});
    connect(action(action::OpenCloudManagementUrl), &QAction::triggered, this,
        &QnWorkbenchCloudHandler::at_openCloudManagementUrlAction_triggered);

    connect(action(action::OpenCloudRegisterUrl), &QAction::triggered, this,
        [this]()
        {
            QDesktopServices::openUrl(m_cloudUrlHelper->createAccountUrl());
        });

    // Forcing logging out if found 'logged out' status.
    // Seems like it can cause double disconnect.
    auto watcher = qnCloudStatusWatcher;
    connect(watcher, &QnCloudStatusWatcher::forcedLogout, this,
        [this]()
        {
            menu()->trigger(action::LogoutFromCloud);
        });
}

QnWorkbenchCloudHandler::~QnWorkbenchCloudHandler()
{
}

void QnWorkbenchCloudHandler::at_loginToCloudAction_triggered()
{
    if (!m_loginToCloudDialog)
    {
        m_loginToCloudDialog = new QnLoginToCloudDialog(mainWindowWidget());
        m_loginToCloudDialog->setLogin(
            nx::vms::client::core::settings()->cloudCredentials().user);
        m_loginToCloudDialog->show();

        connect(m_loginToCloudDialog, &QnLoginToCloudDialog::finished, this,
            [this]()
            {
                m_loginToCloudDialog->deleteLater();
            });
    }

    m_loginToCloudDialog->raise();
    m_loginToCloudDialog->activateWindow();
}

void QnWorkbenchCloudHandler::at_logoutFromCloudAction_triggered()
{
    // Updating login if were logged under temporary credentials.
    qnCloudStatusWatcher->resetCredentials(true);
    nx::vms::client::core::helpers::forgetSavedCloudCredentials(true);
}

void QnWorkbenchCloudHandler::at_openCloudMainUrlAction_triggered()
{
    QDesktopServices::openUrl(m_cloudUrlHelper->mainUrl());
}

void QnWorkbenchCloudHandler::at_openCloudManagementUrlAction_triggered()
{
    QDesktopServices::openUrl(m_cloudUrlHelper->accountManagementUrl());
}
