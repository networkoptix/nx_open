#include "merge_systems_dialog.h"
#include "ui_merge_systems_dialog.h"

#include <QtCore/QUrl>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QMessageBox>

#include <api/app_server_connection.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <ui/common/ui_resource_name.h>
#include <ui/style/custom_style.h>
#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/actions/action_manager.h>
#include <ui/workbench/watchers/workbench_user_watcher.h>
#include <ui/workbench/workbench_context.h>
#include <utils/merge_systems_tool.h>
#include <utils/common/util.h>

QnMergeSystemsDialog::QnMergeSystemsDialog(QWidget *parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::QnMergeSystemsDialog),
    m_mergeTool(new QnMergeSystemsTool(this)),
    m_successfullyFinished(false)
{
    ui->setupUi(this);

    QStringList successMessage;
    successMessage
        << tr("Success!")
        << QString()
        << QString()
        << tr("The system was configured successfully.")
        << tr("The servers from the remote system should appear in your system soon.");
    ui->successLabel->setText(successMessage.join(L'\n'));

    ui->urlComboBox->lineEdit()->setPlaceholderText(tr("http(s)://host:port"));
    m_mergeButton = ui->buttonBox->addButton(QString(), QDialogButtonBox::ActionRole);
    setWarningStyle(ui->errorLabel);
    m_mergeButton->hide();
    ui->buttonBox->button(QDialogButtonBox::Close)->hide();
    ui->buttonBox->button(QDialogButtonBox::Ok)->hide();

    setHelpTopic(this, Qn::Systems_MergeSystems_Help);

    QButtonGroup *buttonGroup = new QButtonGroup(this);
    buttonGroup->addButton(ui->currentSystemRadioButton);
    buttonGroup->addButton(ui->remoteSystemRadioButton);

    connect(ui->urlComboBox,            SIGNAL(activated(int)),             this,   SLOT(at_urlComboBox_activated(int)));
    connect(ui->urlComboBox->lineEdit(),&QLineEdit::editingFinished,        this,   &QnMergeSystemsDialog::at_urlComboBox_editingFinished);
    connect(ui->urlComboBox->lineEdit(),SIGNAL(returnPressed()),            ui->passwordEdit,   SLOT(setFocus()));
    connect(ui->passwordEdit,           &QLineEdit::returnPressed,          this,   &QnMergeSystemsDialog::at_testConnectionButton_clicked);
    connect(ui->testConnectionButton,   &QPushButton::clicked,              this,   &QnMergeSystemsDialog::at_testConnectionButton_clicked);
    connect(m_mergeButton,              &QPushButton::clicked,              this,   &QnMergeSystemsDialog::at_mergeButton_clicked);

    connect(m_mergeTool,                &QnMergeSystemsTool::systemFound,   this,   &QnMergeSystemsDialog::at_mergeTool_systemFound);
    connect(m_mergeTool,                &QnMergeSystemsTool::mergeFinished, this,   &QnMergeSystemsDialog::at_mergeTool_mergeFinished);

    updateKnownSystems();
    updateConfigurationBlock();
}

QnMergeSystemsDialog::~QnMergeSystemsDialog() {}

void QnMergeSystemsDialog::done(int result)
{
    base_type::done(result);

    if (m_successfullyFinished && ui->remoteSystemRadioButton->isChecked())
    {
        m_successfullyFinished = false;

        context()->instance<QnWorkbenchUserWatcher>()->setUserPassword(m_adminPassword);
        QUrl url = QnAppServerConnectionFactory::url();
        url.setPassword(m_adminPassword);
        QnAppServerConnectionFactory::setUrl(url);

        menu()->trigger(QnActions::ReconnectAction);
        context()->instance<QnWorkbenchUserWatcher>()->setReconnectOnPasswordChange(true);
    }
}

QUrl QnMergeSystemsDialog::url() const {
    /* filter unnecessary information from the URL */
    QUrl enteredUrl = QUrl::fromUserInput(ui->urlComboBox->currentText());
    QUrl url;
    url.setScheme(enteredUrl.scheme());
    url.setHost(enteredUrl.host());
    url.setPort(enteredUrl.port());
    return url;
}

QString QnMergeSystemsDialog::password() const {
    return ui->passwordEdit->text();
}

void QnMergeSystemsDialog::updateKnownSystems() {
    ui->urlComboBox->clear();

    foreach (const QnResourcePtr &resource, qnResPool->getAllIncompatibleResources()) {
        QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
        if (!server)
            continue;

        QString url = server->getApiUrl();
        QString label = getResourceName(server);
        QString systemName = server->getSystemName();
        if (!systemName.isEmpty())
            label += lit(" (%1)").arg(systemName);

        ui->urlComboBox->addItem(label, url);
    }

    ui->urlComboBox->setCurrentText(QString());

    ui->currentSystemLabel->setText(tr("You are about to merge the current system %1 with the system").arg(qnCommon->localSystemName()));
    ui->currentSystemRadioButton->setText(tr("%1 (current)").arg(qnCommon->localSystemName()));
}

void QnMergeSystemsDialog::updateErrorLabel(const QString &error) {
    ui->errorLabel->setText(error);
}

void QnMergeSystemsDialog::updateConfigurationBlock() {
    bool found = !m_discoverer.isNull();
    ui->configurationWidget->setEnabled(found);
    m_mergeButton->setEnabled(found);
    if (found)
        m_mergeButton->setFocus();
}

void QnMergeSystemsDialog::at_urlComboBox_activated(int index) {
    if (index == -1)
        return;

    ui->urlComboBox->setCurrentText(ui->urlComboBox->itemData(index).toString());
    ui->passwordEdit->setFocus();
}

void QnMergeSystemsDialog::at_urlComboBox_editingFinished() {
    QUrl url = QUrl::fromUserInput(ui->urlComboBox->currentText());
    if (url.port() == -1)
        url.setPort(DEFAULT_APPSERVER_PORT);
    ui->urlComboBox->setCurrentText(url.toString());
}

void QnMergeSystemsDialog::at_testConnectionButton_clicked() {
    Q_ASSERT(context()->user()->isAdmin());
    if (!context()->user()->isAdmin())
        return;

    m_discoverer.clear();
    m_url.clear();
    m_adminPassword.clear();
    m_successfullyFinished = false;
    updateConfigurationBlock();

    QUrl url = QUrl::fromUserInput(ui->urlComboBox->currentText());
    QString password = ui->passwordEdit->text();

    if ((url.scheme() != lit("http") && url.scheme() != lit("https")) || url.host().isEmpty()) {
        updateErrorLabel(tr("The URL is invalid."));
        updateConfigurationBlock();
        return;
    }

    if (password.isEmpty()) {
        updateErrorLabel(tr("The password cannot be empty."));
        updateConfigurationBlock();
        return;
    }

    m_url = url;
    m_adminPassword = password;
    m_mergeTool->pingSystem(m_url, m_adminPassword);
    ui->buttonBox->showProgress(tr("Testing..."));
}

void QnMergeSystemsDialog::at_mergeButton_clicked() {
    if (!m_discoverer)
        return;

    m_successfullyFinished = false;

    bool ownSettings = ui->currentSystemRadioButton->isChecked();
    ui->credentialsGroupBox->setEnabled(false);
    ui->configurationWidget->setEnabled(false);
    m_mergeButton->setEnabled(false);

    if (!ownSettings)
        context()->instance<QnWorkbenchUserWatcher>()->setReconnectOnPasswordChange(false);

    m_mergeTool->mergeSystem(m_discoverer, m_url, m_adminPassword, ownSettings);
    ui->buttonBox->showProgress(tr("Merging Systems..."));
}

void QnMergeSystemsDialog::at_mergeTool_systemFound(const QnModuleInformation &moduleInformation, const QnMediaServerResourcePtr &discoverer, int errorCode) {
    ui->buttonBox->hideProgress();

    switch (errorCode) {
    case QnMergeSystemsTool::NoError:
    case QnMergeSystemsTool::StarterLicenseError:
    {
        QnMediaServerResourcePtr server = qnResPool->getResourceById<QnMediaServerResource>(moduleInformation.id);
        if (server && server->getStatus() == Qn::Online && moduleInformation.systemName == qnCommon->localSystemName()) {
            if (m_url.host() == lit("localhost") || m_url.host() == lit("127.0.0.1"))
                updateErrorLabel(tr("Use a specific hostname or IP address rather than %1.").arg(m_url.host()));
            else
                updateErrorLabel(tr("This is the current system URL."));
            break;
        }
        m_discoverer = discoverer;
        ui->remoteSystemLabel->setText(moduleInformation.systemName);
        m_mergeButton->setText(tr("Merge with %1").arg(moduleInformation.systemName));
        m_mergeButton->show();
        ui->remoteSystemRadioButton->setText(moduleInformation.systemName);
        updateErrorLabel(QString());
        if (errorCode == QnMergeSystemsTool::StarterLicenseError)
            updateErrorLabel(
            tr("Warning: You are about to merge Systems with START licenses.\n"\
               "As only 1 START license is allowed per System after your merge you will only have 1 START license remaining.\n"\
               "If you understand this and would like to proceed please click Merge to continue.\n")
            );
        break;
    }
    case QnMergeSystemsTool::AuthentificationError:
        updateErrorLabel(tr("The password is invalid."));
        break;
    case QnMergeSystemsTool::VersionError:
        updateErrorLabel(tr("The discovered system %1 has an incompatible version %2.").arg(moduleInformation.systemName).arg(moduleInformation.version.toString()));
        break;
    case QnMergeSystemsTool::SafeModeError:
        updateErrorLabel(tr("The discovered system %1 is in safe mode.").arg(moduleInformation.systemName));
        break;
    default:
        updateErrorLabel(tr("The system was not found."));
        break;
    }

    updateConfigurationBlock();
}

void QnMergeSystemsDialog::at_mergeTool_mergeFinished(
        int errorCode,
        const QnModuleInformation &moduleInformation)
{
    Q_UNUSED(moduleInformation)

    ui->buttonBox->hideProgress();
    ui->credentialsGroupBox->setEnabled(true);

    if (errorCode == QnMergeSystemsTool::NoError)
    {
        m_mergeButton->hide();
        ui->buttonBox->button(QDialogButtonBox::Cancel)->hide();

        const bool reconnectNeeded = ui->remoteSystemRadioButton->isChecked();
        QDialogButtonBox::StandardButton closeButton = reconnectNeeded ? QDialogButtonBox::Ok : QDialogButtonBox::Close;
        ui->buttonBox->button(closeButton)->show();
        ui->buttonBox->button(closeButton)->setFocus();

        ui->reconnectLabel->setVisible(reconnectNeeded);
        ui->stackedWidget->setCurrentWidget(ui->finalPage);
        m_successfullyFinished = true;
    }
    else
    {
        QString message;

        switch (errorCode)
        {
        case QnMergeSystemsTool::AuthentificationError:
            message = tr("The password is invalid.");
            break;
        case QnMergeSystemsTool::VersionError:
            message = tr("System has an incompatible version.");
            break;
        case QnMergeSystemsTool::BackupError:
            message = tr("Could not create a backup of the server database.");
            break;
        case QnMergeSystemsTool::NotFoundError:
            message = tr("System was not found.");
            break;
        case QnMergeSystemsTool::ForbiddenError:
            message = tr("Operation is not permitted.");
            break;
        case QnMergeSystemsTool::SafeModeError:
            message = tr("System is in safe mode.");
            break;
        default:
            break;
        }

        if (!message.isEmpty())
            message.prepend(lit("\n"));

        QnMessageBox::critical(this, tr("Error"), tr("Cannot merge systems.") + message);

        context()->instance<QnWorkbenchUserWatcher>()->setReconnectOnPasswordChange(true);

        updateConfigurationBlock();
    }
}
