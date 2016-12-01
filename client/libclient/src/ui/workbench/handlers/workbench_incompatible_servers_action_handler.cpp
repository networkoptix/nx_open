#include "workbench_incompatible_servers_action_handler.h"

#include <QtWidgets/QInputDialog>
#include <QtCore/QUrl>

#include <core/resource/resource.h>
#include <core/resource/fake_media_server.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>

#include <licensing/license.h>
#include <licensing/remote_licenses.h>

#include <ui/actions/action_manager.h>
#include <ui/actions/action.h>
#include <ui/actions/action_parameter_types.h>
#include <ui/dialogs/merge_systems_dialog.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/dialogs/common/progress_dialog.h>
#include <ui/dialogs/common/session_aware_dialog.h>
#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>
#include <update/connect_to_current_system_tool.h>
#include <utils/merge_systems_tool.h>
#include <utils/merge_systems_common.h>
#include <network/system_helpers.h>

QnWorkbenchIncompatibleServersActionHandler::QnWorkbenchIncompatibleServersActionHandler(
    QObject* parent)
    :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_connectTool(nullptr),
    m_mergeDialog(nullptr)
{
    connect(action(QnActions::ConnectToCurrentSystem), &QAction::triggered, this,
        &QnWorkbenchIncompatibleServersActionHandler::at_connectToCurrentSystemAction_triggered);
    connect(action(QnActions::MergeSystems), &QAction::triggered, this,
        &QnWorkbenchIncompatibleServersActionHandler::at_mergeSystemsAction_triggered);
}

QnWorkbenchIncompatibleServersActionHandler::~QnWorkbenchIncompatibleServersActionHandler()
{
}

void QnWorkbenchIncompatibleServersActionHandler::at_connectToCurrentSystemAction_triggered()
{
    if (m_connectTool)
    {
        QnMessageBox::critical(
            mainWindow(),
            tr("Error"),
            tr("Please wait. Requested servers will be added to your system."));
        return;
    }

    const auto resources = menu()->currentParameters(sender()).resources();
    NX_ASSERT(resources.size() == 1, "We can't connect/merge more then one server");
    if (resources.isEmpty())
        return;

    const auto resource = resources.first();
    const auto status = resource->getStatus();
    if (status != Qn::Incompatible && status != Qn::Unauthorized)
        return;

    const auto serverResource = resource.dynamicCast<QnFakeMediaServerResource>();
    const auto moduleInformation = serverResource->getModuleInformation();

    if (helpers::isCloudSystem(moduleInformation))
    {
        static const auto kStatus = utils::MergeSystemsStatus::dependentSystemBoundToCloud;

        const auto message = utils::MergeSystemsStatus::getErrorMessage(
             kStatus, moduleInformation).prepend(lit("\n"));
        QnMessageBox::critical(mainWindow(), tr("Error"), tr("Cannot merge systems.") + message);
        return;
    }

    connectToCurrentSystem(resource->getId());
}

void QnWorkbenchIncompatibleServersActionHandler::connectToCurrentSystem(
    const QnUuid& target,
    const QString& initialPassword)
{
    if (m_connectTool)
        return;

    if (target.isNull())
        return;

    auto password = initialPassword;

    for (;;)
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
        {
            QnMessageBox::critical(mainWindow(), tr("Error"), tr("Password cannot be empty!"));
            continue;
        }

        break;
    }

    if (!validateStartLicenses(target, password))
        return;

    m_connectTool = new QnConnectToCurrentSystemTool(this);

    auto progressDialog = new QnProgressDialog(mainWindow());
    progressDialog->setAttribute(Qt::WA_DeleteOnClose);
    progressDialog->setCancelButton(nullptr);
    progressDialog->setWindowTitle(tr("Connecting to the current system..."));
    progressDialog->show();

    connect(m_connectTool, &QnConnectToCurrentSystemTool::finished,
        this, &QnWorkbenchIncompatibleServersActionHandler::at_connectTool_finished);
    connect(m_connectTool, &QnConnectToCurrentSystemTool::finished,
        progressDialog, &QnProgressDialog::close);
    connect(m_connectTool, &QnConnectToCurrentSystemTool::progressChanged,
        progressDialog, &QnProgressDialog::setValue);
    connect(m_connectTool, &QnConnectToCurrentSystemTool::stateChanged,
        progressDialog, &QnProgressDialog::setLabelText);
    connect(progressDialog, &QnProgressDialog::canceled,
        m_connectTool, &QnConnectToCurrentSystemTool::cancel);

    m_connectTool->start(target, password);
}

void QnWorkbenchIncompatibleServersActionHandler::at_mergeSystemsAction_triggered()
{
    if (m_mergeDialog)
    {
        m_mergeDialog->raise();
        return;
    }

    m_mergeDialog = new QnSessionAware<QnMergeSystemsDialog>(mainWindow());
    m_mergeDialog->exec();
    delete m_mergeDialog;
}

void QnWorkbenchIncompatibleServersActionHandler::at_connectTool_finished(int errorCode)
{
    m_connectTool->deleteLater();
    m_connectTool->disconnect(this);

    switch (errorCode)
    {
        case QnConnectToCurrentSystemTool::NoError:
            QnMessageBox::information(
                mainWindow(),
                tr("Information"),
                tr("Rejoice! The selected server has been successfully connected to your system!"));
            break;

        case QnConnectToCurrentSystemTool::UpdateFailed:
            QnMessageBox::critical(
                mainWindow(),
                tr("Error"),
                tr("Could not update the selected server.")
                    + L'\n'
                    + m_connectTool->updateResult().errorMessage());
            break;

        case QnConnectToCurrentSystemTool::MergeFailed:
            QnMessageBox::critical(
                mainWindow(),
                tr("Error"),
                tr("Merge failed.") + L'\n' + m_connectTool->mergeErrorMessage());
            break;

        case QnConnectToCurrentSystemTool::Canceled:
            break;
    }
}

bool QnWorkbenchIncompatibleServersActionHandler::validateStartLicenses(
    const QnUuid& target,
    const QString& adminPassword)
{

    const auto licenseHelper = QnLicenseListHelper(qnLicensePool->getLicenses());
    if (licenseHelper.totalLicenseByType(Qn::LC_Start) == 0)
        return true; /* We have no start licenses so all is OK. */

    QnMediaServerResourceList aliveServers;

    const auto server =
        qnResPool->getIncompatibleResourceById(target, true).dynamicCast<QnMediaServerResource>();

    /* Handle this error elsewhere */
    if (!server)
        return true;

    if (!serverHasStartLicenses(server, adminPassword))
        return true;

    const auto message = utils::MergeSystemsStatus::getErrorMessage(
        utils::MergeSystemsStatus::starterLicense, server->getModuleInformation());

    QnMessageBox messageBox(
        QnMessageBox::Warning, Qn::Empty_Help,
        tr("Warning!"), message,
        QDialogButtonBox::Cancel);
    messageBox.addButton(tr("Merge"), QDialogButtonBox::AcceptRole);

    return messageBox.exec() != QDialogButtonBox::Cancel;
}

bool QnWorkbenchIncompatibleServersActionHandler::serverHasStartLicenses(
    const QnMediaServerResourcePtr& server,
    const QString& adminPassword)
{
    QAuthenticator auth;
    auth.setUser(lit("admin"));
    auth.setPassword(adminPassword);

    /* Check if there is a valid starter license in the remote system. */
    const auto remoteLicensesList = remoteLicenses(server->getUrl(), auth);

    /* Warn that some of the licenses will be deactivated. */
    return QnLicenseListHelper(remoteLicensesList).totalLicenseByType(Qn::LC_Start, true) > 0;
}
