#include "workbench_incompatible_servers_action_handler.h"

#include <QtWidgets/QMessageBox>
#include <QtWidgets/QInputDialog>
#include <QtCore/QUrl>

#include <core/resource/resource.h>
#include <ui/actions/action_manager.h>
#include <ui/actions/action.h>
#include <ui/actions/action_parameter_types.h>
#include <ui/dialogs/join_other_system_dialog.h>
#include <ui/dialogs/progress_dialog.h>

#include <update/connect_to_current_system_tool.h>
#include <update/join_system_tool.h>

QnWorkbenchIncompatibleServersActionHandler::QnWorkbenchIncompatibleServersActionHandler(QObject *parent) :
    QObject(parent),
    QnWorkbenchContextAware(parent),
    m_connectToCurrentSystemTool(NULL),
    m_joinSystemTool(NULL),
    m_progressDialog(NULL)
{
    connect(action(Qn::ConnectToCurrentSystem),         SIGNAL(triggered()),    this,   SLOT(at_connectToCurrentSystemAction_triggered()));
    connect(action(Qn::JoinOtherSystem),                SIGNAL(triggered()),    this,   SLOT(at_joinOtherSystemAction_triggered()));
}

void QnWorkbenchIncompatibleServersActionHandler::at_connectToCurrentSystemAction_triggered() {
    QnConnectToCurrentSystemTool *tool = connectToCurrentSystemTool();

    if (tool->isRunning()) {
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

    progressDialog()->setWindowTitle(tr("Connecting to the current system..."));
    progressDialog()->show();

    QnConnectToCurrentSystemTool *tool = connectToCurrentSystemTool();

    connect(tool, &QnConnectToCurrentSystemTool::progressChanged, progressDialog(), &QnProgressDialog::setValue);
    connect(tool, &QnConnectToCurrentSystemTool::stateChanged, progressDialog(), &QnProgressDialog::setLabelText);
    connect(progressDialog(), &QnProgressDialog::canceled, tool, &QnConnectToCurrentSystemTool::cancel);

    connectToCurrentSystemTool()->connectToCurrentSystem(targets, password);
}

void QnWorkbenchIncompatibleServersActionHandler::at_joinOtherSystemAction_triggered() {
    QnJoinSystemTool *tool = joinSystemTool();

    if (tool->isRunning()) {
        QMessageBox::critical(mainWindow(), tr("Error"), tr("Please, wait before the previously requested servers will be added to your system."));
        return;
    }

    QScopedPointer<QnJoinOtherSystemDialog> dialog(new QnJoinOtherSystemDialog(mainWindow()));
    if (dialog->exec() == QDialog::Rejected)
        return;

    QUrl url = dialog->url();
    QString password = dialog->password();

    if ((url.scheme() != lit("http") && url.scheme() != lit("https")) || url.host().isEmpty()) {
        QMessageBox::critical(mainWindow(), tr("Error"), tr("You have entered an invalid url."));
        return;
    }

    if (password.isEmpty()) {
        QMessageBox::critical(mainWindow(), tr("Error"), tr("The password cannot be empty."));
        return;
    }

    progressDialog()->setLabelText(tr("Joining system..."));
    progressDialog()->setInfiniteProgress();
    progressDialog()->show();

    tool->start(url, password);
}

QnConnectToCurrentSystemTool *QnWorkbenchIncompatibleServersActionHandler::connectToCurrentSystemTool() {
    if (!m_connectToCurrentSystemTool) {
        m_connectToCurrentSystemTool = new QnConnectToCurrentSystemTool(context(), this);
        connect(m_connectToCurrentSystemTool, &QnConnectToCurrentSystemTool::finished, this, &QnWorkbenchIncompatibleServersActionHandler::at_connectToCurrentSystemTool_finished);
    }
    return m_connectToCurrentSystemTool;
}

QnJoinSystemTool *QnWorkbenchIncompatibleServersActionHandler::joinSystemTool() {
    if (!m_joinSystemTool) {
        m_joinSystemTool = new QnJoinSystemTool(this);
        connect(m_joinSystemTool, &QnJoinSystemTool::finished, this, &QnWorkbenchIncompatibleServersActionHandler::at_joinSystemTool_finished);
    }
    return m_joinSystemTool;
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

void QnWorkbenchIncompatibleServersActionHandler::at_connectToCurrentSystemTool_finished(int errorCode) {
    progressDialog()->close();

    switch (errorCode) {
    case QnConnectToCurrentSystemTool::NoError:
        QMessageBox::information(progressDialog(), tr("Information"), tr("The selected servers has been successfully connected to your system!"));
        break;
    case QnConnectToCurrentSystemTool::AuthentificationFailed:
        QMessageBox::critical(progressDialog(), tr("Error"), tr("Authentification failed.\nPlease, check the password you have entered."));
        connectToCurrentSystem(connectToCurrentSystemTool()->targets());
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
}

void QnWorkbenchIncompatibleServersActionHandler::at_connectToCurrentSystemTool_canceled() {
    progressDialog()->close();
}

void QnWorkbenchIncompatibleServersActionHandler::at_joinSystemTool_finished(int errorCode) {
    switch (errorCode) {
    case QnJoinSystemTool::NoError:
        QMessageBox::information(progressDialog(), tr("Information"), tr("The selected system has been joined to your system successfully."));
        break;
    case QnJoinSystemTool::Timeout:
        QMessageBox::critical(progressDialog(), tr("Error"), tr("Connection timed out."));
        break;
    case QnJoinSystemTool::HostLookupError:
        QMessageBox::critical(progressDialog(), tr("Error"), tr("The specified host has not been found."));
        break;
    case QnJoinSystemTool::VersionError:
        QMessageBox::critical(progressDialog(), tr("Error"), tr("The target system has the different version.\nYou must update that system before joining."));
        break;
    case QnJoinSystemTool::AuthentificationError:
        QMessageBox::critical(progressDialog(), tr("Error"), tr("Authentification failed.\nPlease, check the password you have entered."));
        break;
    default:
        QMessageBox::critical(progressDialog(), tr("Error"), tr("Could not join the system."));
        break;
    }
    progressDialog()->close();
}
