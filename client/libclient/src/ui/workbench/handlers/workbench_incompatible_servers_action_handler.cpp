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
        QnMessageBox::information(mainWindow(),
            tr("Systems will be merged shortly"),
            tr("Servers from the other System will appear in the resource tree."));
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
    NX_ASSERT(serverResource);
    if (!serverResource)
        return;

    const auto moduleInformation = serverResource->getModuleInformation();

    if (helpers::isCloudSystem(moduleInformation))
    {
        static const auto kStatus = utils::MergeSystemsStatus::dependentSystemBoundToCloud;

        const auto message = utils::MergeSystemsStatus::getErrorMessage(
             kStatus, moduleInformation).prepend(lit("\n"));
        QnMessageBox::critical(mainWindow(), tr("Failed to merge Systems"), message);
        return;
    }

    connectToCurrentSystem(serverResource);
}

void QnWorkbenchIncompatibleServersActionHandler::connectToCurrentSystem(
    const QnFakeMediaServerResourcePtr& server)
{
    NX_ASSERT(server);
    NX_ASSERT(!m_connectTool);
    if (m_connectTool || !server)
        return;

    const auto moduleInformation = server->getModuleInformation();
    QnUuid target = server->getId();
    if (target.isNull())
        return;

    const QString password = helpers::isNewSystem(moduleInformation)
        ? helpers::kFactorySystemPassword
        : requestPassword();

    if (password.isEmpty())
        return;

    if (!validateStartLicenses(server, password))
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
            QnMessageBox::success(mainWindow(), tr("Server connected to the System"));
            break;

        case QnConnectToCurrentSystemTool::UpdateFailed:
            QnMessageBox::critical(mainWindow(),
                tr("Failed to update Server"),
                m_connectTool->updateResult().errorMessage());
            break;

        case QnConnectToCurrentSystemTool::MergeFailed:
            QnMessageBox::critical(mainWindow(),
                tr("Failed to merge Systems"),
                m_connectTool->mergeErrorMessage());
            break;

        case QnConnectToCurrentSystemTool::Canceled:
            break;
    }
}

bool QnWorkbenchIncompatibleServersActionHandler::validateStartLicenses(
    const QnFakeMediaServerResourcePtr& server,
    const QString& adminPassword)
{
    NX_ASSERT(server);
    if (!server)
        return true;

    const auto licenseHelper = QnLicenseListHelper(qnLicensePool->getLicenses());
    if (licenseHelper.totalLicenseByType(Qn::LC_Start) == 0)
        return true; /* We have no start licenses so all is OK. */

    if (!serverHasStartLicenses(server, adminPassword))
        return true;

    const auto message = utils::MergeSystemsStatus::getErrorMessage(
        utils::MergeSystemsStatus::starterLicense, server->getModuleInformation());

    const auto result = QnMessageBox::warning(mainWindow(),
        tr("Total amount of licenses will decrease"), message,
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, QDialogButtonBox::Ok);

    return (result != QDialogButtonBox::Cancel);
}

bool QnWorkbenchIncompatibleServersActionHandler::serverHasStartLicenses(
    const QnMediaServerResourcePtr& server,
    const QString& adminPassword)
{
    QAuthenticator auth;
    auth.setUser(helpers::kFactorySystemUser);
    auth.setPassword(adminPassword);

    /* Check if there is a valid starter license in the remote system. */
    const auto remoteLicensesList = remoteLicenses(server->getUrl(), auth);

    /* Warn that some of the licenses will be deactivated. */
    return QnLicenseListHelper(remoteLicensesList).totalLicenseByType(Qn::LC_Start, true) > 0;
}

QString QnWorkbenchIncompatibleServersActionHandler::requestPassword() const
{
    QString password;
    for (;;)
    {
        QInputDialog dialog(mainWindow());
        dialog.setWindowTitle(tr("Enter Password..."));
        dialog.setLabelText(tr("Administrator Password"));
        dialog.setTextEchoMode(QLineEdit::Password);
        dialog.setTextValue(password);
        setHelpTopic(&dialog, Qn::Systems_ConnectToCurrentSystem_Help);

        if (dialog.exec() != QDialog::Accepted)
            return QString();

        password = dialog.textValue();

        if (password.isEmpty())
        {
            QnMessageBox::warning(mainWindow(), tr("Password cannot be empty!"));
            continue;
        }

        break;
    }

    return password;
}
