#include "workbench_incompatible_servers_action_handler.h"

#include <QtWidgets/QMessageBox>
#include <QtWidgets/QInputDialog>
#include <QtCore/QUrl>

#include <core/resource/resource.h>
#include <ui/actions/action_manager.h>
#include <ui/actions/action.h>
#include <ui/actions/action_parameter_types.h>
#include <ui/dialogs/merge_systems_dialog.h>
#include <ui/dialogs/progress_dialog.h>

#include <update/connect_to_current_system_tool.h>
#include <utils/merge_systems_tool.h>

QnWorkbenchIncompatibleServersActionHandler::QnWorkbenchIncompatibleServersActionHandler(QObject *parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_connectTool(NULL),
    m_progressDialog(NULL)
{
    connect(action(Qn::ConnectToCurrentSystem),         SIGNAL(triggered()),    this,   SLOT(at_connectToCurrentSystemAction_triggered()));
    connect(action(Qn::MergeSystems),                   SIGNAL(triggered()),    this,   SLOT(at_mergeSystemsAction_triggered()));
}

void QnWorkbenchIncompatibleServersActionHandler::at_connectToCurrentSystemAction_triggered() {
    if (m_connectTool) {
        QMessageBox::critical(mainWindow(), tr("Error"), tr("Please, wait before the previously requested servers will be added to your system."));
        return;
    }

    QSet<QUuid> targets;
    foreach (const QnResourcePtr &resource, menu()->currentParameters(sender()).resources()) {
        if (resource->hasFlags(Qn::remote_server) && (resource->getStatus() == Qn::Incompatible || resource->getStatus() == Qn::Unauthorized))
            targets.insert(resource->getId());
    }

    connectToCurrentSystem(targets);
}

void QnWorkbenchIncompatibleServersActionHandler::connectToCurrentSystem(const QSet<QUuid> &targets) {
    if (targets.isEmpty())
        return;

    QString password = QInputDialog::getText(mainWindow(), tr("Enter Password..."), tr("Administrator Password"), QLineEdit::Password);
    if (password.isEmpty())
        return;

    m_connectTool = new QnConnectToCurrentSystemTool(context(), this);

    progressDialog()->setWindowTitle(tr("Connecting to the current system..."));
    progressDialog()->show();

    connect(m_connectTool,      &QnConnectToCurrentSystemTool::finished,        this,                           &QnWorkbenchIncompatibleServersActionHandler::at_connectTool_finished);
    connect(m_connectTool,      &QnConnectToCurrentSystemTool::progressChanged, progressDialog(),               &QnProgressDialog::setValue);
    connect(m_connectTool,      &QnConnectToCurrentSystemTool::stateChanged,    progressDialog(),               &QnProgressDialog::setLabelText);
    connect(progressDialog(),   &QnProgressDialog::canceled,                    m_connectTool,                  &QnConnectToCurrentSystemTool::cancel);

    m_connectTool->start(targets, password);
}

void QnWorkbenchIncompatibleServersActionHandler::at_mergeSystemsAction_triggered() {
    QScopedPointer<QnMergeSystemsDialog> dialog(new QnMergeSystemsDialog(mainWindow()));
    dialog->exec();
}

QnProgressDialog *QnWorkbenchIncompatibleServersActionHandler::progressDialog() {
    if (!m_progressDialog) {
        m_progressDialog = new QnProgressDialog(mainWindow());
        m_progressDialog->setAttribute(Qt::WA_DeleteOnClose);
        m_progressDialog->setCancelButton(NULL);
        m_progressDialog->setModal(true);
    }
    return m_progressDialog;
}

void QnWorkbenchIncompatibleServersActionHandler::at_connectTool_finished(int errorCode) {
    progressDialog()->close();

    switch (errorCode) {
    case QnConnectToCurrentSystemTool::NoError:
        QMessageBox::information(progressDialog(), tr("Information"), tr("The selected servers has been successfully connected to your system!"));
        break;
    case QnConnectToCurrentSystemTool::AuthentificationFailed:
        QMessageBox::critical(progressDialog(), tr("Error"), tr("Authentification failed.\nPlease, check the password you have entered."));
        connectToCurrentSystem(m_connectTool->targets());
        break;
    case QnConnectToCurrentSystemTool::ConfigurationFailed:
        QMessageBox::critical(progressDialog(), tr("Error"), tr("Could not configure the selected servers."));
        break;
    case QnConnectToCurrentSystemTool::UpdateFailed:
        QMessageBox::critical(progressDialog(), tr("Error"), tr("Could not update the selected servers.\nYou can try to update the servers again in the System Administration."));
        break;
    default:
        break;
    }

    delete m_connectTool;
}

void QnWorkbenchIncompatibleServersActionHandler::at_connectToCurrentSystemTool_canceled() {
    progressDialog()->close();
    delete m_connectTool;
}

void QnWorkbenchIncompatibleServersActionHandler::at_joinSystemTool_finished(int errorCode) {
    switch (errorCode) {
    case QnMergeSystemsTool::NoError:
        QMessageBox::information(progressDialog(), tr("Information"), tr("The selected system has been joined to your system successfully."));
        break;
    case QnMergeSystemsTool::VersionError:
        QMessageBox::critical(progressDialog(), tr("Error"), tr("The target system has the different version.\nYou must update that system before joining."));
        break;
    case QnMergeSystemsTool::AuthentificationError:
        QMessageBox::critical(progressDialog(), tr("Error"), tr("Authentification failed.\nPlease, check the password you have entered."));
        break;
    default:
        QMessageBox::critical(progressDialog(), tr("Error"), tr("Could not join the system."));
        break;
    }
    progressDialog()->close();
}
