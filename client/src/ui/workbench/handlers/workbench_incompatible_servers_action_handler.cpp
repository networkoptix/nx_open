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
#include <ui/dialogs/workbench_state_dependent_dialog.h>
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
    connect(action(Qn::ConnectToCurrentSystem),         SIGNAL(triggered()),    this,   SLOT(at_connectToCurrentSystemAction_triggered()));
    connect(action(Qn::MergeSystems),                   SIGNAL(triggered()),    this,   SLOT(at_mergeSystemsAction_triggered()));
}

QnWorkbenchIncompatibleServersActionHandler::~QnWorkbenchIncompatibleServersActionHandler() {}

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

void QnWorkbenchIncompatibleServersActionHandler::connectToCurrentSystem(const QSet<QnUuid> &targets, const QString &initialUser, const QString &initialPassword) {
    if (m_connectTool)
        return;

    if (targets.isEmpty())
        return;

    QString user = initialUser.isEmpty() ? lit("admin") : initialUser;
    QString password = initialPassword;

    forever {
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
            QMessageBox::critical(mainWindow(), tr("Error"), tr("Password cannot be empty!"));
        else
            break;
    }

    m_connectTool = new QnConnectToCurrentSystemTool(context(), this);

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

    m_connectTool->start(targets, user, password);
}

void QnWorkbenchIncompatibleServersActionHandler::at_mergeSystemsAction_triggered() {
    if (m_mergeDialog) {
        m_mergeDialog->raise();
        return;
    }

    m_mergeDialog = new QnWorkbenchStateDependentDialog<QnMergeSystemsDialog>(mainWindow());
    m_mergeDialog->exec();
    delete m_mergeDialog;
}

void QnWorkbenchIncompatibleServersActionHandler::at_connectTool_finished(int errorCode) {
    m_connectTool->deleteLater();
    m_connectTool->QObject::disconnect(this);

    switch (errorCode) {
    case QnConnectToCurrentSystemTool::NoError:
        QMessageBox::information(mainWindow(), tr("Information"), tr("The selected servers has been successfully connected to your system!"));
        break;
    case QnConnectToCurrentSystemTool::AuthentificationFailed: {
        QMessageBox::critical(mainWindow(), tr("Error"), tr("Authentification failed.\nPlease, check the password you have entered."));
        QSet<QnUuid> targets = m_connectTool->targets();
        QString user = m_connectTool->user();
        QString password = m_connectTool->password();
        delete m_connectTool;
        connectToCurrentSystem(targets, user, password);
        break;
    }
    case QnConnectToCurrentSystemTool::ConfigurationFailed:
        QMessageBox::critical(mainWindow(), tr("Error"), tr("Could not configure the selected servers."));
        break;
    case QnConnectToCurrentSystemTool::UpdateFailed:
        QMessageBox::critical(mainWindow(), tr("Error"), tr("Could not update the selected servers.\nYou can try to update the servers again in the System Administration."));
        break;
    default:
        break;
    }
}
