#include "workbench_cloud_handler.h"

#include <QtGui/QDesktopServices>

#include <common/common_module.h>
#include <utils/common/connective.h>
#include <watchers/cloud_status_watcher.h>
#include <helpers/cloud_url_helper.h>
#include <client/client_settings.h>
#include <ui/actions/actions.h>
#include <ui/dialogs/login_to_cloud_dialog.h>

class QnWorkbenchCloudHandlerPrivate : public Connective<QObject>
{
    QnWorkbenchCloudHandler *q_ptr;
    Q_DECLARE_PUBLIC(QnWorkbenchCloudHandler)

public:
    QnWorkbenchCloudHandlerPrivate(QnWorkbenchCloudHandler *parent);

    void at_loginToCloudAction_triggered();
    void at_logoutFromCloudAction_triggered();
    void at_openCloudMainUrlAction_triggered();
    void at_openCloudManagementUrlAction_triggered();

public:
    QPointer<QnLoginToCloudDialog> loginToCloudDialog;
};

QnWorkbenchCloudHandler::QnWorkbenchCloudHandler(QObject *parent)
    : base_type(parent)
    , QnWorkbenchContextAware(parent)
    , d_ptr(new QnWorkbenchCloudHandlerPrivate(this))
{
    Q_D(QnWorkbenchCloudHandler);

    connect(action(QnActions::LoginToCLoud),           &QAction::triggered,    d,  &QnWorkbenchCloudHandlerPrivate::at_loginToCloudAction_triggered);
    connect(action(QnActions::LogoutFromCloud),        &QAction::triggered,    d,  &QnWorkbenchCloudHandlerPrivate::at_logoutFromCloudAction_triggered);
    connect(action(QnActions::OpenCloudMainUrl),       &QAction::triggered,    d,  &QnWorkbenchCloudHandlerPrivate::at_openCloudMainUrlAction_triggered);
    connect(action(QnActions::OpenCloudManagementUrl), &QAction::triggered,    d,  &QnWorkbenchCloudHandlerPrivate::at_openCloudManagementUrlAction_triggered);

    connect(action(QnActions::OpenCloudRegisterUrl), &QAction::triggered, this,
        []() { QDesktopServices::openUrl(QnCloudUrlHelper::createAccountUrl()); });
}

QnWorkbenchCloudHandler::~QnWorkbenchCloudHandler()
{
}

QnWorkbenchCloudHandlerPrivate::QnWorkbenchCloudHandlerPrivate(QnWorkbenchCloudHandler *parent)
    : Connective<QObject>(parent)
    , q_ptr(parent)
{
}

void QnWorkbenchCloudHandlerPrivate::at_loginToCloudAction_triggered()
{
    Q_Q(QnWorkbenchCloudHandler);

    if (!loginToCloudDialog)
    {
        loginToCloudDialog = new QnLoginToCloudDialog(q->mainWindow());
        loginToCloudDialog->setLogin(qnSettings->cloudLogin());
        loginToCloudDialog->show();

        connect(loginToCloudDialog, &QnLoginToCloudDialog::finished, this, [this]()
        {
            loginToCloudDialog->deleteLater();
        });
    }

    loginToCloudDialog->raise();
    loginToCloudDialog->activateWindow();
}

void QnWorkbenchCloudHandlerPrivate::at_logoutFromCloudAction_triggered()
{
    QnCloudStatusWatcher *cloudStatusWatcher = qnCommon->instance<QnCloudStatusWatcher>();
    qnSettings->setCloudPassword(QString());
    cloudStatusWatcher->setCloudPassword(QString());
}

void QnWorkbenchCloudHandlerPrivate::at_openCloudMainUrlAction_triggered()
{
    QDesktopServices::openUrl(QnCloudUrlHelper::mainUrl());
}

void QnWorkbenchCloudHandlerPrivate::at_openCloudManagementUrlAction_triggered()
{
    QDesktopServices::openUrl(QnCloudUrlHelper::accountManagementUrl());
}
