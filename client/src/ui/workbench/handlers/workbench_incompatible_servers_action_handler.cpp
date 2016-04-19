#include "workbench_incompatible_servers_action_handler.h"

#include <QtWidgets/QInputDialog>
#include <QtCore/QUrl>

#include <core/resource/resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>

#include <licensing/license.h>
#include <licensing/remote_licenses.h>

#include <ui/actions/action_manager.h>
#include <ui/actions/action.h>
#include <ui/actions/action_parameter_types.h>
#include <ui/dialogs/merge_systems_dialog.h>
#include <ui/dialogs/common/progress_dialog.h>
#include <ui/dialogs/common/workbench_state_dependent_dialog.h>
#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>

#include <update/connect_to_current_system_tool.h>
#include <utils/merge_systems_tool.h>

QnWorkbenchIncompatibleServersActionHandler::QnWorkbenchIncompatibleServersActionHandler(QObject *parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_connectTool(0),
    m_mergeDialog(0)
{
    connect(action(QnActions::ConnectToCurrentSystem),         SIGNAL(triggered()),    this,   SLOT(at_connectToCurrentSystemAction_triggered()));
    connect(action(QnActions::MergeSystems),                   SIGNAL(triggered()),    this,   SLOT(at_mergeSystemsAction_triggered()));
}

QnWorkbenchIncompatibleServersActionHandler::~QnWorkbenchIncompatibleServersActionHandler() {}

void QnWorkbenchIncompatibleServersActionHandler::at_connectToCurrentSystemAction_triggered()
{
    if (m_connectTool)
    {
        QnMessageBox::critical(mainWindow(), tr("Error"), tr("Please wait. Requested servers will be added to your system."));
        return;
    }

    QSet<QnUuid> targets;
    for (const QnResourcePtr &resource: menu()->currentParameters(sender()).resources())
    {
        Qn::ResourceStatus status = resource->getStatus();

        if (status == Qn::Incompatible || status == Qn::Unauthorized)
            targets.insert(resource->getId());
    }

    connectToCurrentSystem(targets);
}

void QnWorkbenchIncompatibleServersActionHandler::connectToCurrentSystem(
        const QSet<QnUuid> &targets,
        const QString &initialPassword)
{
    if (m_connectTool)
        return;

    if (targets.isEmpty())
        return;

    QString password = initialPassword;

    forever
    {
        QInputDialog dialog(mainWindow());
        dialog.setWindowTitle(tr("Enter Password..."));
        dialog.setLabelText(tr("Administrator Password"));
        dialog.setTextEchoMode(QLineEdit::Password);
        dialog.setTextValue(password);
        setHelpTopic(&dialog, Qn::Systems_ConnectToCurrentSystem_Help);

        if (dialog.exec() != QDialog::Accepted)
            return;

        password = dialog.textValue();

        if (password.isEmpty())
            QnMessageBox::critical(mainWindow(), tr("Error"), tr("Password cannot be empty!"));
        else
            break;
    }

    if (!validateStartLicenses(targets, password))
        return;

    m_connectTool = new QnConnectToCurrentSystemTool(this);

    QnProgressDialog *progressDialog = new QnProgressDialog(mainWindow());
    progressDialog->setAttribute(Qt::WA_DeleteOnClose);
    progressDialog->setCancelButton(NULL);
    progressDialog->setWindowTitle(tr("Connecting to the current system..."));
    progressDialog->show();

    connect(m_connectTool,      &QnConnectToCurrentSystemTool::finished,        progressDialog,        &QnProgressDialog::close);
    connect(m_connectTool,      &QnConnectToCurrentSystemTool::finished,        this,                  &QnWorkbenchIncompatibleServersActionHandler::at_connectTool_finished);
    connect(m_connectTool,      &QnConnectToCurrentSystemTool::progressChanged, progressDialog,        &QnProgressDialog::setValue);
    connect(m_connectTool,      &QnConnectToCurrentSystemTool::stateChanged,    progressDialog,        &QnProgressDialog::setLabelText);
    connect(progressDialog,     &QnProgressDialog::canceled,                    m_connectTool,         &QnConnectToCurrentSystemTool::cancel);

    m_connectTool->start(targets, password);
}

void QnWorkbenchIncompatibleServersActionHandler::at_mergeSystemsAction_triggered()
{
    if (m_mergeDialog)
    {
        m_mergeDialog->raise();
        return;
    }

    m_mergeDialog = new QnWorkbenchStateDependentDialog<QnMergeSystemsDialog>(mainWindow());
    m_mergeDialog->exec();
    delete m_mergeDialog;
}

void QnWorkbenchIncompatibleServersActionHandler::at_connectTool_finished(int errorCode)
{
    m_connectTool->deleteLater();
    m_connectTool->QObject::disconnect(this);

    switch (errorCode)
    {
    case QnConnectToCurrentSystemTool::NoError:
        QnMessageBox::information(mainWindow(), tr("Information"), tr("Rejoice! Selected servers have been successfully connected to your system!"));
        break;
    case QnConnectToCurrentSystemTool::AuthentificationFailed: {
        QnMessageBox::critical(mainWindow(), tr("Error"), tr("Authentication failed.") + L'\n' + tr("Please, check the password you have entered."));
        QSet<QnUuid> targets = m_connectTool->targets();
        QString password = m_connectTool->adminPassword();
        delete m_connectTool;
        connectToCurrentSystem(targets, password);
        break;
    }
    case QnConnectToCurrentSystemTool::ConfigurationFailed:
        QnMessageBox::critical(mainWindow(), tr("Error"), tr("Could not configure the selected servers."));
        break;
    case QnConnectToCurrentSystemTool::UpdateFailed:
        QnMessageBox::critical(mainWindow(), tr("Error"), tr("Could not update the selected servers.") + L'\n' + tr("You can try to update the servers again in the System Administration dialog."));
        break;
    default:
        break;
    }
}

bool QnWorkbenchIncompatibleServersActionHandler::validateStartLicenses(
        const QSet<QnUuid> &targets,
        const QString &adminPassword)
{

    QnLicenseListHelper helper(qnLicensePool->getLicenses());
    if (helper.totalLicenseByType(Qn::LC_Start) == 0)
        return true; /* We have no start licenses so all is OK. */

    QnMediaServerResourceList aliveServers;

    for (const QnUuid &id: targets)
    {
        QnMediaServerResourcePtr server = qnResPool->getIncompatibleResourceById(id, true).dynamicCast<QnMediaServerResource>();
        if (server)
            aliveServers << server;
    }

    /* Handle this error elsewhere */
    if (aliveServers.isEmpty())
        return true;

    bool serversWithStartLicenses = std::any_of(aliveServers.cbegin(), aliveServers.cend(),
            [this, &adminPassword](const QnMediaServerResourcePtr &server)
            {
                return serverHasStartLicenses(server, adminPassword);
            }
    );

    /* Check if all OK */
    if (!serversWithStartLicenses)
        return true;

    QString message = tr("Warning: You are about to merge Systems with START licenses.\n"\
        "As only 1 START license is allowed per System after your merge you will only have 1 START license remaining.\n"\
        "If you understand this and would like to proceed please click Merge to continue.\n");

    QnMessageBox messageBox(QnMessageBox::Warning, 0, tr("Warning!"), message, QDialogButtonBox::Cancel);
    messageBox.addButton(tr("Merge"), QDialogButtonBox::AcceptRole);

    return messageBox.exec() != QDialogButtonBox::Cancel;
}

bool QnWorkbenchIncompatibleServersActionHandler::serverHasStartLicenses(
        const QnMediaServerResourcePtr &server,
        const QString &adminPassword)
{
    QAuthenticator auth;
    auth.setUser(lit("admin"));
    auth.setPassword(adminPassword);

    /* Check if there is a valid starter license in the remote system. */
    QnLicenseList remoteLicensesList = remoteLicenses(server->getUrl(), auth);

    /* Warn that some of the licenses will be deactivated. */
    QnLicenseListHelper remoteHelper(remoteLicensesList);
    return remoteHelper.totalLicenseByType(Qn::LC_Start, true) > 0;
}
