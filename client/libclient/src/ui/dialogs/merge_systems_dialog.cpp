#include "merge_systems_dialog.h"
#include "ui_merge_systems_dialog.h"

#include <QtCore/QUrl>
#include <QtWidgets/QButtonGroup>

#include <api/app_server_connection.h>
#include <common/common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/resource_display_info.h>

#include <client/client_settings.h>

#include <ui/dialogs/common/message_box.h>
#include <ui/style/custom_style.h>
#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/actions/action_manager.h>
#include <ui/workbench/watchers/workbench_user_watcher.h>
#include <ui/workbench/workbench_context.h>
#include <utils/merge_systems_tool.h>
#include <utils/common/util.h>
#include <utils/common/app_info.h>
#include <api/global_settings.h>

#include <nx/utils/string.h>
#include <network/system_helpers.h>

namespace {

static const int kMaxSystemNameLength = 20;

}

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

        context()->instance<QnWorkbenchUserWatcher>()->setUserName(m_remoteOwnerCredentials.user());
        context()->instance<QnWorkbenchUserWatcher>()->setUserPassword(m_remoteOwnerCredentials.password());

        QUrl url = QnAppServerConnectionFactory::url();
        url.setUserName(m_remoteOwnerCredentials.user());
        url.setPassword(m_remoteOwnerCredentials.password());
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

void QnMergeSystemsDialog::updateKnownSystems()
{
    ui->urlComboBox->clear();

    for (const QnMediaServerResourcePtr& server: qnResPool->getAllIncompatibleResources().filtered<QnMediaServerResource>())
    {
        QString url = server->getApiUrl().toString();
        QString label = QnResourceDisplayInfo(server).toString(qnSettings->extraInfoInTree());
        QString systemName = server->getModuleInformation().systemName;
        if (!systemName.isEmpty())
            label += lit(" (%1)").arg(systemName);

        ui->urlComboBox->addItem(label, url);
    }

    ui->urlComboBox->setCurrentText(QString());

    const QString displayName = nx::utils::elideString(
        qnGlobalSettings->systemName(), kMaxSystemNameLength);

    //TODO: #GDM #tr translators would go crazy
    ui->currentSystemLabel->setText(
        tr("You are about to merge the current system %1 with the system")
        .arg(displayName));

    ui->currentSystemRadioButton->setText(tr("%1 (current)").arg(displayName));
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

void QnMergeSystemsDialog::at_testConnectionButton_clicked()
{
    NX_ASSERT(context()->user()->isOwner());
    if (!context()->user()->isOwner())
        return;

    m_discoverer.clear();
    m_url.clear();
    m_remoteOwnerCredentials = QAuthenticator();
    m_successfullyFinished = false;
    updateConfigurationBlock();

    QUrl url = QUrl::fromUserInput(ui->urlComboBox->currentText());
    QString login = ui->loginEdit->text();
    QString password = ui->passwordEdit->text();

    if ((url.scheme() != lit("http") && url.scheme() != lit("https")) || url.host().isEmpty()) {
        updateErrorLabel(tr("The URL is invalid."));
        updateConfigurationBlock();
        return;
    }

    if (login.isEmpty()) {
        updateErrorLabel(tr("The login cannot be empty."));
        updateConfigurationBlock();
        return;
    }

    if (password.isEmpty()) {
        updateErrorLabel(tr("The password cannot be empty."));
        updateConfigurationBlock();
        return;
    }

    m_url = url;
    m_remoteOwnerCredentials.setUser(login);
    m_remoteOwnerCredentials.setPassword(password);
    m_mergeTool->pingSystem(m_url, m_remoteOwnerCredentials);
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

    m_mergeTool->mergeSystem(m_discoverer, m_url, m_remoteOwnerCredentials, ownSettings);
    ui->buttonBox->showProgress(tr("Merging Systems..."));
}

void QnMergeSystemsDialog::at_mergeTool_systemFound(
    utils::MergeSystemsStatus::Value mergeStatus,
    const QnModuleInformation& moduleInformation,
    const QnMediaServerResourcePtr& discoverer)
{
    ui->buttonBox->hideProgress();

    if (mergeStatus != utils::MergeSystemsStatus::ok
        && mergeStatus != utils::MergeSystemsStatus::starterLicense)
    {
        updateErrorLabel(
            utils::MergeSystemsStatus::getErrorMessage(mergeStatus, moduleInformation));
        updateConfigurationBlock();
        return;
    }

    const auto server = qnResPool->getResourceById<QnMediaServerResource>(
        moduleInformation.id);
    if (server && server->getStatus() == Qn::Online
        && helpers::serverBelongsToCurrentSystem(moduleInformation))
    {
        if (m_url.host() == lit("localhost") || QHostAddress(m_url.host()).isLoopback())
        {
            updateErrorLabel(
                tr("Use a specific hostname or IP address rather than %1.").arg(m_url.host()));
        }
        else
        {
            updateErrorLabel(tr("This is the current system URL."));
        }

        return;
    }

    m_discoverer = discoverer;
    ui->remoteSystemLabel->setText(moduleInformation.systemName);
    m_mergeButton->setText(tr("Merge with %1").arg(moduleInformation.systemName));
    m_mergeButton->show();
    ui->remoteSystemRadioButton->setText(moduleInformation.systemName);
    updateErrorLabel(QString());

    if (mergeStatus == utils::MergeSystemsStatus::starterLicense)
    {
        updateErrorLabel(
            utils::MergeSystemsStatus::getErrorMessage(mergeStatus, moduleInformation));
    }

    updateConfigurationBlock();
}

void QnMergeSystemsDialog::at_mergeTool_mergeFinished(
    utils::MergeSystemsStatus::Value mergeStatus,
    const QnModuleInformation& moduleInformation)
{
    ui->buttonBox->hideProgress();
    ui->credentialsGroupBox->setEnabled(true);

    if (mergeStatus == utils::MergeSystemsStatus::ok)
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
        return;
    }

    auto message = utils::MergeSystemsStatus::getErrorMessage(mergeStatus, moduleInformation);
    if (!message.isEmpty())
        message.prepend(lit("\n"));

    QnMessageBox::critical(this, tr("Error"), tr("Cannot merge systems.") + message);

    context()->instance<QnWorkbenchUserWatcher>()->setReconnectOnPasswordChange(true);

    updateConfigurationBlock();
}
