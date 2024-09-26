// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "merge_systems_dialog.h"
#include "ui_merge_systems_dialog.h"

#include <QtWidgets/QButtonGroup>

#include <api/server_rest_connection.h>
#include <client_core/client_core_module.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource_display_info.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <network/system_helpers.h>
#include <nx/network/http/http_types.h>
#include <nx/utils/string.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/network/remote_connection_user_interaction_delegate.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/utils/connection_url_parser.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/licensing/customer_support.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/other_servers/other_server_display_info.h>
#include <nx/vms/client/desktop/other_servers/other_servers_manager.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_logon/logic/context_current_user_watcher.h>
#include <nx/vms/client/desktop/system_logon/logic/fresh_session_token_helper.h>
#include <nx/vms/client/desktop/system_merge/merge_systems_tool.h>
#include <nx/vms/client/desktop/system_merge/merge_systems_validator.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/common/system_settings.h>
#include <nx_ec/abstract_ec_connection.h>
#include <ui/common/palette.h>
#include <ui/workbench/workbench_context.h>
#include <utils/common/util.h>

namespace {

constexpr int kMaxSystemNameLength = 20;

const QSize kIconSize(20, 20);

const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kIconSubstitutions = {
    {QIcon::Normal, {.primary = "red_l2"}},
};

} // namespace

namespace nx::vms::client::desktop {

MergeSystemsDialog::MergeSystemsDialog(QWidget* parent, std::unique_ptr<Delegate> delegate) :
    base_type(parent),
    ui(new Ui::MergeSystemsDialog),
    m_delegate(std::move(delegate)),
    m_mergeTool(new MergeSystemsTool(
        this,
        qnClientCoreModule->networkModule()->certificateVerifier(),
        m_delegate.get(),
        appContext()->coreSettings()->locale().toStdString()))
{
    ui->setupUi(this);
    setButtonBox(ui->buttonBox);
    setHelpTopic(this, HelpTopic::Id::Systems_MergeSystems);

    ui->warnWidget->setHidden(true);
    ui->warnIconLabel->setPixmap(qnSkin->pixmap("16x16/Solid/attention.svg"));
    setPaletteColor(ui->warningLabel, QPalette::WindowText, core::colorTheme()->color("yellow"));
    setPaletteColor(ui->contactSupportLabel, QPalette::WindowText, core::colorTheme()->color("yellow"));

    CustomerSupport customerSupport(systemContext());
    const QUrl url(customerSupport.systemContact.address.rawData);
    ui->contactSupportLabel->setText(tr("It is recommended to contact %1 before proceeding.")
        .arg(nx::vms::common::html::link(tr("support"), url)));
    ui->contactSupportLabel->setOpenExternalLinks(url.isValid());

    buttonBox()->setStandardButtons(QDialogButtonBox::Cancel);
    m_mergeButton = buttonBox()->addButton(QString(), QDialogButtonBox::ActionRole);
    // Do not join strings to avoid translatable string duplication.
    m_mergeButton->setText(tr("Merge with %1").arg("..."));
    setAccentStyle(m_mergeButton);

    QButtonGroup* buttonGroup = new QButtonGroup(this);
    buttonGroup->addButton(ui->currentSystemRadioButton);
    buttonGroup->addButton(ui->remoteSystemRadioButton);
    ui->currentSystemRadioButton->setChecked(true);

    const QString kRemoteSystemRadioButtonDefaultText = tr("Remote");
    ui->remoteSystemRadioButton->setText(kRemoteSystemRadioButtonDefaultText);

    auto enableCredentialsControls =
        [this]()
        {
            ui->passwordEdit->setEnabled(true);
            ui->loginEdit->setEnabled(true);
        };

    connect(ui->urlComboBox, &QComboBox::editTextChanged, this, enableCredentialsControls);
    connect(ui->urlComboBox, SIGNAL(activated(int)), this, SLOT(at_urlComboBox_activated(int)));
    connect(ui->urlComboBox->lineEdit(), &QLineEdit::editingFinished,
        this, &MergeSystemsDialog::at_urlComboBox_editingFinished);
    connect(ui->urlComboBox->lineEdit(), SIGNAL(returnPressed()),
        ui->passwordEdit, SLOT(setFocus()));

    connect(ui->passwordEdit->lineEdit(), &QLineEdit::returnPressed,
        this, &MergeSystemsDialog::at_testConnectionButton_clicked);
    connect(ui->testConnectionButton, &QPushButton::clicked,
        this, &MergeSystemsDialog::at_testConnectionButton_clicked);
    connect(m_mergeButton, &QPushButton::clicked, this, &MergeSystemsDialog::at_mergeButton_clicked);

    connect(m_mergeTool, &MergeSystemsTool::systemFound,
        this, &MergeSystemsDialog::at_mergeTool_systemFound);
    connect(m_mergeTool, &MergeSystemsTool::mergeFinished,
        this, &MergeSystemsDialog::at_mergeTool_mergeFinished);

    connect(ui->urlComboBox, &QComboBox::currentTextChanged, this,
        [this, kRemoteSystemRadioButtonDefaultText]
        {
            ui->remoteSystemRadioButton->setText(kRemoteSystemRadioButtonDefaultText);
        });

    connect(ui->urlComboBox, &QComboBox::currentTextChanged,
        this, &MergeSystemsDialog::resetStateToDefault);
    connect(ui->loginEdit, &QLineEdit::textChanged,
        this, &MergeSystemsDialog::resetStateToDefault);
    connect(ui->passwordEdit, &PasswordInputField::textChanged,
        this, &MergeSystemsDialog::resetStateToDefault);
    connect(ui->currentSystemRadioButton, &QRadioButton::clicked,
        this, &MergeSystemsDialog::resetStateToDefault);
    connect(ui->remoteSystemRadioButton, &QRadioButton::clicked,
        this, &MergeSystemsDialog::resetStateToDefault);

    updateKnownSystems();
    resetStateToDefault();
}

MergeSystemsDialog::~MergeSystemsDialog()
{
}

nx::utils::Url MergeSystemsDialog::url() const {
    /* filter unnecessary information from the URL */
    nx::utils::Url enteredUrl = nx::utils::Url::fromUserInput(ui->urlComboBox->currentText());
    nx::utils::Url url;
    url.setScheme(enteredUrl.scheme());
    url.setHost(enteredUrl.host());
    url.setPort(enteredUrl.port());
    return url;
}

QString MergeSystemsDialog::password() const {
    return ui->passwordEdit->text();
}

void MergeSystemsDialog::updateKnownSystems()
{
    ui->urlComboBox->clear();

    std::multimap<QString, QString> labelUrlMap;

    for (const nx::Uuid& otherServerId: systemContext()->otherServersManager()->getServers())
    {
        QString url = systemContext()->otherServersManager()->getUrl(otherServerId).toString();
        QString label = OtherServerDisplayInfo(otherServerId, systemContext()->otherServersManager())
            .toString(appContext()->localSettings()->resourceInfoLevel());

        const auto moduleInformation =
            systemContext()->otherServersManager()->getModuleInformationWithAddresses(otherServerId);

        QString systemName = helpers::getSystemName(moduleInformation);
        if (!systemName.isEmpty())
            label += nx::format(" (%1)", systemName);

        labelUrlMap.emplace(label, url);
    }

    for (const auto& [label, url] : labelUrlMap)
        ui->urlComboBox->addItem(label, url);

    ui->urlComboBox->setCurrentText(QString());

    const QString displayName = nx::utils::elideString(
        system()->globalSettings()->systemName(), kMaxSystemNameLength);

    ui->currentSystemRadioButton->setText(tr("Current (%1)").arg(displayName));
}

void MergeSystemsDialog::updateStatusLabel(const QString& error, bool isError)
{
    ui->statusWidget->setHidden(error.isEmpty());
    ui->statusLabel->setText(error);

    if (isError)
    {
        setWarningStyle(ui->statusLabel);
        ui->statusIconLabel->setPixmap(
            qnSkin->icon("20x20/Solid/error.svg", kIconSubstitutions).pixmap(kIconSize));
    }
    else
    {
        auto palette = ui->statusLabel->palette();
        setCustomStyle(&palette, core::colorTheme()->color("light4"));
        ui->statusLabel->setPalette(palette);

        ui->statusIconLabel->setPixmap(qnSkin->pixmap("20x20/Solid/success.svg"));
    }
}

void MergeSystemsDialog::setConfigurationAllowed(bool value)
{
    ui->credentialsWidget->setEnabled(value);
}

void MergeSystemsDialog::updateMergeButtonAvailability()
{
    const bool found = m_targetModule.has_value();
    m_mergeButton->setEnabled(found);
    if (found)
        m_mergeButton->setFocus();
}

void MergeSystemsDialog::processRemoteSystemInfo(
    const nx::vms::api::ModuleInformation& moduleInformation)
{
    const bool isNewSystem = helpers::isNewSystem(moduleInformation);
    if (isNewSystem)
    {
        ui->currentSystemRadioButton->setChecked(true);
        ui->loginEdit->setText(helpers::kFactorySystemUser);
        ui->passwordEdit->clear();
    }

    ui->remoteSystemRadioButton->setEnabled(!isNewSystem);
    ui->loginEdit->setEnabled(!isNewSystem);
    ui->passwordEdit->setEnabled(!isNewSystem);

    const QString systemName = helpers::getSystemName(moduleInformation);
    ui->remoteSystemRadioButton->setText(tr("Remote (%1)").arg(systemName));

    m_mergeButton->setText(tr("Merge with %1").arg(systemName));
}

void MergeSystemsDialog::resetStateToDefault()
{
    ui->statusWidget->setHidden(true);
    m_targetModule.reset();
    updateMergeButtonAvailability();
}

void MergeSystemsDialog::at_urlComboBox_activated(int index)
{
    if (index == -1)
        return;

    ui->urlComboBox->setCurrentText(ui->urlComboBox->itemData(index).toString());
    ui->passwordEdit->setFocus();
}

void MergeSystemsDialog::at_urlComboBox_editingFinished()
{
    auto url = parseConnectionUrlFromUserInput(
        ui->urlComboBox->currentText());

    if (!url.empty())
        url.setScheme(nx::network::http::kSecureUrlSchemeName);

    ui->urlComboBox->setCurrentText(url.toString());
}

void MergeSystemsDialog::at_testConnectionButton_clicked()
{
    if (!NX_ASSERT(system()->user()->isAdministrator()))
        return;

    m_mergeContextId = nx::Uuid();
    m_url.clear();
    m_targetModule.reset();
    m_remoteOwnerCredentials = nx::network::http::Credentials();

    updateMergeButtonAvailability();

    nx::utils::Url url = nx::utils::Url::fromUserInput(ui->urlComboBox->currentText());
    QString login = ui->loginEdit->text();
    QString password = ui->passwordEdit->text();

    if (!nx::network::http::isUrlScheme(url.scheme()) || url.host().isEmpty()) {
        updateStatusLabel(tr("URL is invalid."));
        updateMergeButtonAvailability();
        return;
    }

    if (login.isEmpty()) {
        updateStatusLabel(tr("The login cannot be empty."));
        updateMergeButtonAvailability();
        return;
    }

    m_url = url;
    m_remoteOwnerCredentials = nx::network::http::PasswordCredentials(
        login.toStdString(),
        password.isEmpty()
            ? helpers::kFactorySystemPassword.toStdString()
            : password.toStdString());

    m_mergeContextId = m_mergeTool->pingSystem(
        system()->currentServer(),
        m_url,
        m_remoteOwnerCredentials,
        MergeSystemsTool::DryRunSettings {
            .ownSettings = ui->currentSystemRadioButton->isChecked() }
        );

    setConfigurationAllowed(false);
    ui->buttonBox->showProgress(tr("Testing..."));
}

void MergeSystemsDialog::at_mergeButton_clicked()
{
    if (!m_targetModule || m_mergeContextId.isNull())
        return;

    const bool ownSettings = ui->currentSystemRadioButton->isChecked();
    const QString currentSystemName = system()->globalSettings()->systemName();
    const QString targetSystemName = ui->remoteSystemRadioButton->text();

    auto sessionTokenHelper = FreshSessionTokenHelper::makeHelper(
        this,
        tr("Merge Sites"),
        tr("Enter your account password to merge Sites"),
        tr("Merge", "Merge two Sites together (dialog button text)"),
        FreshSessionTokenHelper::ActionType::merge);

    const auto ownerSessionToken = sessionTokenHelper->refreshToken();
    if (!ownerSessionToken)
        return;

    setConfigurationAllowed(false);
    m_mergeButton->setEnabled(false);

    if (!ownSettings)
        workbenchContext()->instance<ContextCurrentUserWatcher>()->setReconnectOnPasswordChange(false);

    if (m_mergeTool->mergeSystem(m_mergeContextId, ownerSessionToken->value, ownSettings))
        ui->buttonBox->showProgress(tr("Merging Sites..."));
}

void MergeSystemsDialog::at_mergeTool_systemFound(
    MergeSystemsStatus mergeStatus,
    const QString& errorText,
    const nx::vms::api::ModuleInformation& moduleInformation,
    const nx::vms::api::MergeStatusReply& reply)
{
    ui->buttonBox->hideProgress();
    setConfigurationAllowed(true);
    if (!moduleInformation.id.isNull())
        processRemoteSystemInfo(moduleInformation);

    ui->warnWidget->setHidden(reply.warnings.empty());
    QStringList warnings;
    for (const auto& warning: reply.warnings)
        warnings << QString::fromStdString(warning);
    ui->warningLabel->setText(warnings.join(". "));

    if (!allowsToMerge(mergeStatus))
    {
        const QString statusLabel = errorText.isEmpty()
            ? MergeSystemsTool::getErrorMessage(mergeStatus, moduleInformation)
            : errorText;
        updateStatusLabel(statusLabel);
        return;
    }

    const auto server = system()->resourcePool()->getResourceById<QnMediaServerResource>(
        moduleInformation.id);
    if (server
        && server->getStatus() == nx::vms::api::ResourceStatus::online
        && helpers::serverBelongsToCurrentSystem(moduleInformation, system()))
    {
        if (m_url.host() == lit("localhost") || QHostAddress(m_url.host()).isLoopback())
        {
            updateStatusLabel(
                tr("Use a specific hostname or IP address rather than %1.").arg(m_url.host()));
        }
        else
        {
            updateStatusLabel(tr("This is the current Site URL."));
        }

        return;
    }

    m_targetModule = moduleInformation;
    updateStatusLabel(tr("Success"), /*isError*/ false);
    updateMergeButtonAvailability();
}

void MergeSystemsDialog::at_mergeTool_mergeFinished(
    MergeSystemsStatus mergeStatus,
    const QString& errorText,
    const nx::vms::api::ModuleInformation& moduleInformation)
{
    ui->buttonBox->hideProgress();
    setConfigurationAllowed(true);

    if (mergeStatus == MergeSystemsStatus::ok)
    {
        const bool reconnectNeeded = ui->remoteSystemRadioButton->isChecked();
        QString successMessage = tr(
            "Servers from the other Site will appear in the resource "
            "tree when the database synchronization is finished.");
        if (reconnectNeeded)
            successMessage.append('\n').append(tr("You will be reconnected."));

        auto messageBox = new QnMessageBox(
            QnMessageBox::Icon::Information,
            tr("Sites will be merged shortly"),
            successMessage,
            QnDialogButtonBox::StandardButton::Ok,
            QnDialogButtonBox::StandardButton::Ok,
            this);
        messageBox->setEscapeButton(QnDialogButtonBox::StandardButton::Ok);

        connect(messageBox, &QDialog::finished, this, &QDialog::accept);
        messageBox->open();

        if (reconnectNeeded)
        {
            // Current admin credentials are replaced by target admin credentials at this point.
            // But it is not known which credentials should be updated (digest or token).
            // Therefore just trigger reconnect.
            workbenchContext()->instance<ContextCurrentUserWatcher>()->setReconnectOnPasswordChange(true);
            menu()->trigger(menu::ReconnectAction);
        }
    }
    else
    {
        workbenchContext()->instance<ContextCurrentUserWatcher>()->setReconnectOnPasswordChange(true);

        QString message = errorText.isEmpty()
            ? MergeSystemsTool::getErrorMessage(mergeStatus, moduleInformation)
            : errorText;
        if (!message.isEmpty())
            message.prepend(lit("\n"));

        QnSessionAwareMessageBox::critical(this, tr("Failed to merge Sites"), message);

        updateMergeButtonAvailability();
    }
}

} // namespace nx::vms::client::desktop
