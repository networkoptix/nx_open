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
    m_connectProgressDialog(NULL)
{
    connect(action(Qn::ConnectToCurrentSystem),         SIGNAL(triggered()),    this,   SLOT(at_connectToCurrentSystemAction_triggered()));
    connect(action(Qn::MergeSystems),                   SIGNAL(triggered()),    this,   SLOT(at_mergeSystemsAction_triggered()));
}

void QnWorkbenchIncompatibleServersActionHandler::at_connectToCurrentSystemAction_triggered() {
    if (m_connectTool) {
        QMessageBox::critical(mainWindow(), tr("Error"), tr("Please, wait before the previously requested servers will be added to your system."));
        return;
    }

    QSet<QnUuid> targets;
    foreach (const QnResourcePtr &resource, menu()->currentParameters(sender()).resources()) {
        if (resource->hasFlags(Qn::remote_server) && (resource->getStatus() == Qn::Incompatible || resource->getStatus() == Qn::Unauthorized))
            targets.insert(resource->getId());
    }

    connectToCurrentSystem(targets);
}

void QnWorkbenchIncompatibleServersActionHandler::connectToCurrentSystem(const QSet<QnUuid> &targets) {
    if (targets.isEmpty())
        return;

    QString password = QInputDialog::getText(mainWindow(), tr("Enter Password..."), tr("Administrator Password"), QLineEdit::Password);
    if (password.isEmpty())
        return;

    m_connectTool = new QnConnectToCurrentSystemTool(context(), this);

    m_connectProgressDialog = new QnProgressDialog(mainWindow());
    m_connectProgressDialog->setAttribute(Qt::WA_DeleteOnClose);
    m_connectProgressDialog->setCancelButton(NULL);
    m_connectProgressDialog->setWindowTitle(tr("Connecting to the current system..."));
    m_connectProgressDialog->show();

    connect(m_connectTool,      &QnConnectToCurrentSystemTool::finished,        this,                           &QnWorkbenchIncompatibleServersActionHandler::at_connectTool_finished);
    connect(m_connectTool,      &QnConnectToCurrentSystemTool::finished,        m_connectProgressDialog,        &QnProgressDialog::close);
    connect(m_connectTool,      &QnConnectToCurrentSystemTool::progressChanged, m_connectProgressDialog,        &QnProgressDialog::setValue);
    connect(m_connectTool,      &QnConnectToCurrentSystemTool::stateChanged,    m_connectProgressDialog,        &QnProgressDialog::setLabelText);
    connect(m_connectProgressDialog,    &QnProgressDialog::canceled,            m_connectTool,                  &QnConnectToCurrentSystemTool::cancel);

    m_connectTool->start(targets, password);
}

void QnWorkbenchIncompatibleServersActionHandler::at_mergeSystemsAction_triggered() {
    if (m_mergeDialog) {
        m_mergeDialog->raise();
        return;
    }

    m_mergeDialog = new QnMergeSystemsDialog(mainWindow());
    m_mergeDialog->exec();
    delete m_mergeDialog;
}

void QnWorkbenchIncompatibleServersActionHandler::at_connectTool_finished(int errorCode) {
    switch (errorCode) {
    case QnConnectToCurrentSystemTool::NoError:
        QMessageBox::information(m_connectProgressDialog, tr("Information"), tr("The selected servers has been successfully connected to your system!"));
        break;
    case QnConnectToCurrentSystemTool::AuthentificationFailed:
        QMessageBox::critical(m_connectProgressDialog, tr("Error"), tr("Authentification failed.\nPlease, check the password you have entered."));
        connectToCurrentSystem(m_connectTool->targets());
        break;
    case QnConnectToCurrentSystemTool::ConfigurationFailed:
        QMessageBox::critical(m_connectProgressDialog, tr("Error"), tr("Could not configure the selected servers."));
        break;
    case QnConnectToCurrentSystemTool::UpdateFailed:
        QMessageBox::critical(m_connectProgressDialog, tr("Error"), tr("Could not update the selected servers.\nYou can try to update the servers again in the System Administration."));
        break;
    default:
        break;
    }

    delete m_connectTool;
}
