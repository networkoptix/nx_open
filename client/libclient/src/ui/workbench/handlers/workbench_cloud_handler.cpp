#include "workbench_cloud_handler.h"

#include <QtGui/QDesktopServices>

#include <api/app_server_connection.h>

#include <core/resource/user_resource.h>

#include <common/common_module.h>
#include <client/client_settings.h>

#include <helpers/cloud_url_helper.h>

#include <ui/actions/actions.h>
#include <ui/actions/action_parameters.h>
#include <ui/actions/action_manager.h>
#include <ui/dialogs/login_to_cloud_dialog.h>
#include <ui/workbench/workbench_context.h>

#include <watchers/cloud_status_watcher.h>

QnWorkbenchCloudHandler::QnWorkbenchCloudHandler(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    connect(action(QnActions::LoginToCloud), &QAction::triggered, this,
        &QnWorkbenchCloudHandler::at_loginToCloudAction_triggered);
    connect(action(QnActions::LogoutFromCloud), &QAction::triggered, this,
        &QnWorkbenchCloudHandler::at_logoutFromCloudAction_triggered);
    connect(action(QnActions::OpenCloudMainUrl), &QAction::triggered, this,
        &QnWorkbenchCloudHandler::at_openCloudMainUrlAction_triggered);
    connect(action(QnActions::OpenCloudManagementUrl), &QAction::triggered, this,
        &QnWorkbenchCloudHandler::at_openCloudManagementUrlAction_triggered);

    connect(action(QnActions::OpenCloudRegisterUrl), &QAction::triggered, this,
        []()
        {
            QDesktopServices::openUrl(QnCloudUrlHelper::createAccountUrl());
        });
}

QnWorkbenchCloudHandler::~QnWorkbenchCloudHandler()
{
}

void QnWorkbenchCloudHandler::at_loginToCloudAction_triggered()
{
    if (!m_loginToCloudDialog)
    {
        m_loginToCloudDialog = new QnLoginToCloudDialog(mainWindow());
        m_loginToCloudDialog->setLogin(qnSettings->cloudLogin());
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
    qnSettings->setCloudPassword(QString());
    qnCloudStatusWatcher->setCloudPassword(QString());

    QString currentLogin = QnAppServerConnectionFactory::url().userName();
    if (currentLogin.isEmpty())
        return;

    if (qnCloudStatusWatcher->cloudLogin() == currentLogin)
        menu()->trigger(QnActions::DisconnectAction,
            QnActionParameters().withArgument(Qn::ForceRole, true));
}

void QnWorkbenchCloudHandler::at_openCloudMainUrlAction_triggered()
{
    QDesktopServices::openUrl(QnCloudUrlHelper::mainUrl());
}

void QnWorkbenchCloudHandler::at_openCloudManagementUrlAction_triggered()
{
    QDesktopServices::openUrl(QnCloudUrlHelper::accountManagementUrl());
}
