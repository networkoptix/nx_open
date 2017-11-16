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
#include <nx/client/desktop/ui/actions/action_manager.h>
#include <ui/workbench/watchers/workbench_user_watcher.h>
#include <ui/workbench/workbench_context.h>
#include <utils/merge_systems_tool.h>
#include <utils/common/util.h>
#include <utils/common/app_info.h>
#include <api/global_settings.h>

#include <nx/utils/string.h>
#include <network/system_helpers.h>

using namespace nx::client::desktop::ui;

namespace {

static const int kMaxSystemNameLength = 20;

}

QnMergeSystemsDialog::QnMergeSystemsDialog(QWidget *parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::QnMergeSystemsDialog),
    m_mergeTool(new QnMergeSystemsTool(this))
{
    ui->setupUi(this);

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

    auto enableCredentialsControls = [this]
        {
            ui->passwordEdit->setEnabled(true);
            ui->loginEdit->setEnabled(true);
        };

    connect(ui->urlComboBox,    &QComboBox::editTextChanged, this, enableCredentialsControls);


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


nx::utils::Url QnMergeSystemsDialog::url() const {
    /* filter unnecessary information from the URL */
    nx::utils::Url enteredUrl = nx::utils::Url::fromUserInput(ui->urlComboBox->currentText());
    nx::utils::Url url;
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

    for (const auto& server: resourcePool()->getIncompatibleServers())
    {
        QString url = server->getApiUrl().toString();
        QString label = QnResourceDisplayInfo(server).toString(qnSettings->extraInfoInTree());

        const auto moduleInformation = server->getModuleInformation();

        QString systemName = helpers::isNewSystem(moduleInformation)
            ? tr("New Server")
            : moduleInformation.systemName;

        if (!systemName.isEmpty())
            label += lit(" (%1)").arg(systemName);

        ui->urlComboBox->addItem(label, url);
    }

    ui->urlComboBox->setCurrentText(QString());

    const QString displayName = nx::utils::elideString(
        qnGlobalSettings->systemName(), kMaxSystemNameLength);

    // TODO: #GDM #tr translators would go crazy
    ui->currentSystemLabel->setText(
        tr("You are about to merge the current System %1 with System")
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
    nx::utils::Url url = nx::utils::Url::fromUserInput(ui->urlComboBox->currentText());
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

    updateConfigurationBlock();

    nx::utils::Url url = nx::utils::Url::fromUserInput(ui->urlComboBox->currentText());
    QString login = ui->loginEdit->text();
    QString password = ui->passwordEdit->text();

    if ((url.scheme() != lit("http") && url.scheme() != lit("https")) || url.host().isEmpty()) {
        updateErrorLabel(tr("URL is invalid."));
        updateConfigurationBlock();
        return;
    }

    if (login.isEmpty()) {
        updateErrorLabel(tr("The login cannot be empty."));
        updateConfigurationBlock();
        return;
    }

    m_url = url;
    m_remoteOwnerCredentials.setUser(login);
    m_remoteOwnerCredentials.setPassword(password.isEmpty()
        ? helpers::kFactorySystemPassword
        : password);
    m_mergeTool->pingSystem(m_url, m_remoteOwnerCredentials);
    ui->credentialsGroupBox->setEnabled(false);
    ui->buttonBox->showProgress(tr("Testing..."));
}

void QnMergeSystemsDialog::at_mergeButton_clicked()
{
    if (!m_discoverer)
        return;

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
    ui->credentialsGroupBox->setEnabled(true);

    if ((mergeStatus == utils::MergeSystemsStatus::ok)
        && helpers::isCloudSystem(moduleInformation)
        && helpers::isCloudSystem(discoverer->getModuleInformation()))
    {
        mergeStatus = utils::MergeSystemsStatus::bothSystemBoundToCloud;
    }

    if (mergeStatus != utils::MergeSystemsStatus::ok
        && mergeStatus != utils::MergeSystemsStatus::starterLicense)
    {
        updateErrorLabel(
            utils::MergeSystemsStatus::getErrorMessage(mergeStatus, moduleInformation));
        updateConfigurationBlock();
        return;
    }

    const auto server = resourcePool()->getResourceById<QnMediaServerResource>(
        moduleInformation.id);
    if (server && server->getStatus() == Qn::Online
        && helpers::serverBelongsToCurrentSystem(moduleInformation, commonModule()))
    {
        if (m_url.host() == lit("localhost") || QHostAddress(m_url.host()).isLoopback())
        {
            updateErrorLabel(
                tr("Use a specific hostname or IP address rather than %1.").arg(m_url.host()));
        }
        else
        {
            updateErrorLabel(tr("This is the current System URL."));
        }

        return;
    }

    m_discoverer = discoverer;

    bool isNewSystem = helpers::isNewSystem(moduleInformation);
    if (isNewSystem)
    {
        ui->currentSystemRadioButton->setChecked(true);
        ui->loginEdit->setText(helpers::kFactorySystemUser);
        ui->passwordEdit->clear();
    }

    ui->remoteSystemRadioButton->setEnabled(!isNewSystem);
    ui->loginEdit->setEnabled(!isNewSystem);
    ui->passwordEdit->setEnabled(!isNewSystem);
    const QString systemName = isNewSystem
        ? tr("New Server")
        : moduleInformation.systemName;

    ui->remoteSystemLabel->setText(systemName);
    ui->remoteSystemRadioButton->setText(systemName);
    m_mergeButton->setText(tr("Merge with %1").arg(systemName));
    m_mergeButton->show();

    if (mergeStatus == utils::MergeSystemsStatus::starterLicense)
    {
        updateErrorLabel(
            utils::MergeSystemsStatus::getErrorMessage(mergeStatus, moduleInformation));
    }
    else
    {
        updateErrorLabel(QString());
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
        const bool reconnectNeeded = ui->remoteSystemRadioButton->isChecked();

        QString successMessage = tr("Servers from the other System will appear in the resource "
            "tree when the database synchronization is finished.");

        if (reconnectNeeded)
            successMessage += L'\n' + tr("You will be reconnected.");

        hide(); //< Do not close dialog here as it will be deleted in messagebox event loop.

        if (reconnectNeeded)
        {
            context()->instance<QnWorkbenchUserWatcher>()->setUserName(m_remoteOwnerCredentials.user());
            context()->instance<QnWorkbenchUserWatcher>()->setUserPassword(m_remoteOwnerCredentials.password());

            if (auto connection = QnAppServerConnectionFactory::ec2Connection())
            {
                nx::utils::Url url = connection->connectionInfo().ecUrl;
                url.setUserName(m_remoteOwnerCredentials.user());
                url.setPassword(m_remoteOwnerCredentials.password());
                connection->updateConnectionUrl(url);
            }

            menu()->trigger(action::ReconnectAction);
            context()->instance<QnWorkbenchUserWatcher>()->setReconnectOnPasswordChange(true);
        }

        QnMessageBox::success(mainWindowWidget(),
            tr("Systems will be merged shortly"),
            successMessage);

        accept();
    }
    else
    {
        auto message = utils::MergeSystemsStatus::getErrorMessage(mergeStatus, moduleInformation);
        if (!message.isEmpty())
            message.prepend(lit("\n"));

        QnMessageBox::critical(this, tr("Failed to merge Systems"), message);

        context()->instance<QnWorkbenchUserWatcher>()->setReconnectOnPasswordChange(true);

        updateConfigurationBlock();
    }
}
