// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "security_settings_widget.h"
#include "ui_security_settings_widget.h"

#include <chrono>

#include <api/server_rest_connection.h>
#include <common/common_module.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/api/data/system_settings.h>
#include <nx/vms/client/desktop/common/dialogs/repeated_password_dialog.h>
#include <nx/vms/client/desktop/common/widgets/hint_button.h>
#include <nx/vms/client/desktop/help/help_handler.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/watermark/watermark_edit_settings.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/text/human_readable.h>
#include <nx/vms/text/time_strings.h>
#include <ui/common/read_only.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/workaround/widgets_signals_workaround.h>

using namespace nx::vms::common;
using namespace std::chrono;
using namespace std::chrono_literals;

namespace {

constexpr auto kSessionLengthAlertLimit = std::chrono::hours(720);

} // namespace

namespace nx::vms::client::desktop {

SecuritySettingsWidget::SecuritySettingsWidget(QWidget* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::SecuritySettingsWidget),
    m_archiveEncryptionPasswordDialog(new RepeatedPasswordDialog(this))
{
    using HumanReadable = nx::vms::text::HumanReadable;

    ui->setupUi(this);
    resetWarnings();
    setWarningStyle(ui->archiveEncryptionWarning);

    m_archiveEncryptionPasswordDialog->setWindowTitle(tr("Archive encryption password"));
    m_archiveEncryptionPasswordDialog->setHeaderText(tr(
        "The encryption password will be required to restore the archive on another system."
        "\nCaution: This password cannot be reset. If you lose it, the archive will be unrecoverable."));

    setHelpTopic(ui->useHttpsOnlyCamerasCheckBox, HelpTopic::Id::ConnectToCamerasOverOnlyHttps);
    ui->useHttpsOnlyCamerasCheckBox->setHint(ui->useHttpsOnlyCamerasCheckBox->text());

    setHelpTopic(ui->displayWatermarkCheckBox, HelpTopic::Id::UserWatermark);
    ui->displayWatermarkCheckBox->setHint(tr(
        "Watermarks will be displayed over live, archive and exported videos for non-power users"
        " only. You and other power users will not see them."));

    setHelpTopic(ui->auditTrailCheckBox, HelpTopic::Id::AuditTrail);
    ui->auditTrailCheckBox->setHint(tr("Tracks and logs all user actions."));

    setHelpTopic(ui->limitSessionLengthCheckBox, HelpTopic::Id::LaunchingAndClosing);
    ui->limitSessionLengthCheckBox->setHint(tr(
        "Local and LDAP users will be automatically logged out if their session exceeds the"
        " specified duration."));

    ui->forceTrafficEncryptionCheckBox->setHint(
        tr("Does not affect the connections established by server."));
    setHelpTopic(ui->forceTrafficEncryptionCheckBox, HelpTopic::Id::ForceSecureConnections);

    setHelpTopic(ui->forceVideoTrafficEncryptionCheckBox,
        HelpTopic::Id::EnableEncryptedVideoTraffic);
    ui->forceVideoTrafficEncryptionCheckBox->setHint(tr(
        "Enables RTSP traffic encryption."));

    setHelpTopic(ui->archiveEncryptionGroupBox, HelpTopic::Id::EnableArchiveEncryption);
    const auto archiveEncryptionHint = HintButton::createGroupBoxHint(ui->archiveEncryptionGroupBox);
    archiveEncryptionHint->setHintText(tr(
        "Encrypts archive data to prevent it from being viewed outside of the system. "
        "You will not be required to enter the encryption password to view the video archive "
        "within this system."));

    connect(ui->watermarkSettingsButton, &QPushButton::pressed, this,
        [this]
        {
            // We need to pass settings with watermark enabled
            auto settingsCopy = m_watermarkSettings;
            settingsCopy.useWatermark = true;

            if (editWatermarkSettings(settingsCopy, this))
            {
                m_watermarkSettings = settingsCopy;
                emit hasChangesChanged();
            }
        });

    // This should go before connecting to hasChangesChanged!
    auto updateWatermarkState =
        [this](int state)
        {
            m_watermarkSettings.useWatermark = (state == Qt::Checked);
            ui->watermarkSettingsButton->setEnabled(state == Qt::Checked);
        };
    connect(ui->displayWatermarkCheckBox, &QCheckBox::stateChanged,
        this, updateWatermarkState);
    // Need to sync checkbox to button, loadDataToUi() will do the rest.
    updateWatermarkState(ui->displayWatermarkCheckBox->checkState());

    ui->limitSessionUnitsComboBox->addItem(
        QnTimeStrings::fullSuffixCapitalized(QnTimeStrings::Suffix::Minutes),
        QVariant::fromValue(1min));
    ui->limitSessionUnitsComboBox->addItem(
        QnTimeStrings::fullSuffixCapitalized(QnTimeStrings::Suffix::Hours),
        QVariant::fromValue(1h));
    ui->limitSessionUnitsComboBox->addItem(
        QnTimeStrings::fullSuffixCapitalized(QnTimeStrings::Suffix::Days),
        QVariant::fromValue(24h));
    ui->limitSessionUnitsComboBox->setCurrentIndex(2);
    ui->limitSessionValueSpinBox->setValue(
        std::chrono::duration_cast<std::chrono::days>(kSessionLengthAlertLimit).count());

    ui->limitSessionAlertLabel->setText(tr("Unlimited user session lifetime threatens overall"
        " System security and may lead to degradation in System performance"));
    ui->limitSessionLengthAlertLabel->setText(tr(
        "The recommended maximum user session lifetime is %1",
        "%1 is the time span with time units")
            .arg(HumanReadable::timeSpan(kSessionLengthAlertLimit, HumanReadable::Days)));

    const auto updateArchiveEncryptionControls =
        [this](bool enabled) { ui->archiveEncryptionWarning->setVisible(enabled); };

    connect(ui->archiveEncryptionGroupBox, &QGroupBox::toggled, this,
        [this, updateArchiveEncryptionControls](bool checked)
        {
            if (checked)
            {
                if (m_archivePasswordState == ArchivePasswordState::notSet
                    || m_archivePasswordState == ArchivePasswordState::failedToSet)
                {
                    showArchiveEncryptionPasswordDialog(/*viaButton*/ false);
                }
            }
            else
            {
                m_archivePasswordState = ArchivePasswordState::notSet;
            }
            updateArchiveEncryptionControls(checked);
        });

    updateArchiveEncryptionControls(ui->archiveEncryptionGroupBox->isChecked());

    connect(ui->changeArchivePasswordButton, &QPushButton::clicked, this,
        [this]() { showArchiveEncryptionPasswordDialog(/*viaButton*/ true); });

    connect(m_archiveEncryptionPasswordDialog, &QDialog::finished, this,
        [this](int result)
        {
            if (result == QDialog::Accepted)
            {
                m_archivePasswordState = ArchivePasswordState::changed;
            }
            else if (!m_archiveEncryptionPasswordDialogOpenedViaButton)
            {
                loadEncryptionSettingsToUi();
            }
        });

    connect(ui->digestAlertLabel, &AlertLabel::linkActivated, this,
        [] { HelpHandler::openHelpTopic(HelpTopic::Id::SessionAndDigestAuth); });
    connect(ui->manageUsersButton, &QPushButton::clicked,
        this, &SecuritySettingsWidget::manageUsers);

    // All hasChangesChanged connections should go after initial control updates.
    connect(ui->auditTrailCheckBox, &QCheckBox::stateChanged, this,
        &QnAbstractPreferencesWidget::hasChangesChanged);

    connect(ui->forceTrafficEncryptionCheckBox, &QCheckBox::stateChanged, this,
        &QnAbstractPreferencesWidget::hasChangesChanged);
    connect(ui->useHttpsOnlyCamerasCheckBox, &QCheckBox::stateChanged, this,
        &QnAbstractPreferencesWidget::hasChangesChanged);
    connect(ui->forceVideoTrafficEncryptionCheckBox, &QCheckBox::stateChanged, this,
        &QnAbstractPreferencesWidget::hasChangesChanged);

    connect(ui->forceVideoTrafficEncryptionCheckBox, &QCheckBox::clicked,
        ui->forceVideoEncryptionWarning, &QWidget::setVisible);
    connect(ui->useHttpsOnlyCamerasCheckBox, &QCheckBox::clicked,
        ui->useHttpsOnlyCamerasWarning, &QWidget::setVisible);

    connect(ui->displayWatermarkCheckBox, &QCheckBox::stateChanged, this,
        &QnAbstractPreferencesWidget::hasChangesChanged);

    connect(ui->limitSessionLengthCheckBox, &QCheckBox::toggled, this,
        &SecuritySettingsWidget::updateLimitSessionControls);
    connect(ui->limitSessionValueSpinBox, QnSpinboxIntValueChanged, this,
        &SecuritySettingsWidget::updateLimitSessionControls);
    connect(ui->limitSessionUnitsComboBox, QnComboboxCurrentIndexChanged, this,
        &SecuritySettingsWidget::updateLimitSessionControls);

    connect(ui->archiveEncryptionGroupBox, &QGroupBox::toggled, this,
        &QnAbstractPreferencesWidget::hasChangesChanged);

    connect(ui->showServersInTreeCheckBox, &QCheckBox::stateChanged, this,
        &QnAbstractPreferencesWidget::hasChangesChanged);
}

SecuritySettingsWidget::~SecuritySettingsWidget()
{
}

void SecuritySettingsWidget::updateLimitSessionControls()
 {
     const bool hasSessionLimit = ui->limitSessionLengthCheckBox->isChecked();
     ui->limitSessionDetailsWidget->setEnabled(hasSessionLimit);
     ui->limitSessionAlertLabel->setVisible(!hasSessionLimit);

     const auto limit = calculateSessionLimit();
     ui->limitSessionLengthAlertLabel->setVisible(limit && (*limit > kSessionLengthAlertLimit));

     emit hasChangesChanged();
 }

std::optional<std::chrono::seconds> SecuritySettingsWidget::calculateSessionLimit() const
{
    if (!ui->limitSessionLengthCheckBox->isChecked())
        return std::nullopt;

    return ui->limitSessionUnitsComboBox->currentData().value<seconds>()
        * ui->limitSessionValueSpinBox->value();
}

void SecuritySettingsWidget::showArchiveEncryptionPasswordDialog(bool viaButton)
{
    m_archiveEncryptionPasswordDialogOpenedViaButton = viaButton;
    m_archiveEncryptionPasswordDialog->clearInputFields();
    m_archiveEncryptionPasswordDialog->show();
}

void SecuritySettingsWidget::loadDataToUi()
{
    const auto systemSetting = systemContext()->systemSettings();
    ui->auditTrailCheckBox->setChecked(systemSetting->auditTrailEnabled);

    ui->forceTrafficEncryptionCheckBox->setChecked(systemSetting->trafficEncryptionForced);
    ui->useHttpsOnlyCamerasCheckBox->setChecked(systemSetting->useHttpsOnlyForCameras);
    ui->forceVideoTrafficEncryptionCheckBox->setChecked(
        systemSetting->videoTrafficEncryptionForced);

    m_watermarkSettings = systemSetting->watermarkSettings;
    ui->displayWatermarkCheckBox->setChecked(m_watermarkSettings.useWatermark);

    const auto sessionTimeoutLimit = systemSetting->sessionLimitS;
    ui->limitSessionLengthCheckBox->setChecked(sessionTimeoutLimit.has_value());
    if (sessionTimeoutLimit)
    {
        for (int index = ui->limitSessionUnitsComboBox->count() - 1; index >= 0; --index)
        {
            const int secondsInPeriod =
                ui->limitSessionUnitsComboBox->itemData(index).value<seconds>().count();
            if (sessionTimeoutLimit->count() % secondsInPeriod == 0)
            {
                ui->limitSessionUnitsComboBox->setCurrentIndex(index);
                ui->limitSessionValueSpinBox->setValue(
                    sessionTimeoutLimit->count() / secondsInPeriod);
                break;
            }
        }
    }

    if (!systemSetting->videoTrafficEncryptionForced)
        ui->forceVideoEncryptionWarning->hide();
    if (!systemSetting->useHttpsOnlyForCameras)
        ui->useHttpsOnlyCamerasWarning->hide();

    loadEncryptionSettingsToUi();
    loadUserInfoToUi();
    updateLimitSessionControls();

    ui->showServersInTreeCheckBox->setChecked(systemSetting->showServersInTreeForNonAdmins);
}

void SecuritySettingsWidget::loadEncryptionSettingsToUi()
{
    const auto systemSetting = systemContext()->systemSettings();
    const bool storageEncryption = systemSetting->storageEncryption;
    if (storageEncryption)
    {
        if (m_archivePasswordState != ArchivePasswordState::failedToSet)
            m_archivePasswordState = ArchivePasswordState::set;
    }
    else
    {
        m_archivePasswordState = ArchivePasswordState::notSet;
    }
    ui->archiveEncryptionGroupBox->setChecked(storageEncryption);
}

void SecuritySettingsWidget::loadUserInfoToUi()
{
    size_t digestCount = 0;
    size_t userCount = 0;
    const auto users = resourcePool()->getResources<QnUserResource>();

    for (const auto& user: users)
    {
        ++userCount;
        if (user->shouldDigestAuthBeUsed())
            ++digestCount;
    }

    if (digestCount)
    {
        QString alertText =
            tr("%n out of %1 users are allowed to use digest authentication (not secure).",
                "%n is digest user count, %1 is total user count", digestCount).arg(userCount);
        alertText.append(" ");
        alertText.append(nx::vms::common::html::localLink(tr("Learn more.")));
        ui->digestAlertLabel->setText(alertText);
    }

    ui->digestAlertWidget->setVisible(digestCount);
}

void SecuritySettingsWidget::applyChanges()
{
    if (!hasChanges())
        return;

    const auto systemSetting = systemContext()->systemSettings();
    systemSetting->auditTrailEnabled = ui->auditTrailCheckBox->isChecked();
    systemSetting->trafficEncryptionForced = ui->forceTrafficEncryptionCheckBox->isChecked();
    systemSetting->useHttpsOnlyForCameras = ui->useHttpsOnlyCamerasCheckBox->isChecked();
    systemSetting->videoTrafficEncryptionForced =
        ui->forceVideoTrafficEncryptionCheckBox->isChecked();
    systemSetting->watermarkSettings = m_watermarkSettings;
    systemSetting->sessionLimitS = calculateSessionLimit();
    systemSetting->storageEncryption = ui->archiveEncryptionGroupBox->isChecked();
    systemSetting->showServersInTreeForNonAdmins = ui->showServersInTreeCheckBox->isChecked();
    resetWarnings();

    const auto callback = nx::utils::guarded(this,
        [this](bool success, rest::Handle, const rest::ServerConnection::EmptyResponseType&)
        {
            if (!success)
            {
                m_archivePasswordState = ArchivePasswordState::failedToSet;
                QnMessageBox::critical(mainWindowWidget(),
                    tr("Failed to set archive encryption password"));
                return;
            }
            m_archivePasswordState = ArchivePasswordState::set;
        });

    if (m_archivePasswordState == ArchivePasswordState::changed
        || m_archivePasswordState == ArchivePasswordState::failedToSet)
    {
        auto api = connectedServerApi();
        if (NX_ASSERT(api))
        {
            api->setStorageEncryptionPassword(
                m_archiveEncryptionPasswordDialog->password(),
                /*makeCurrent*/ true,
                /*salt*/ QByteArray(),
                callback,
                thread());
        }
    }
}

bool SecuritySettingsWidget::hasChanges() const
{
    if (isReadOnly())
        return false;

    const auto systemSetting = systemContext()->systemSettings();

    return (ui->auditTrailCheckBox->isChecked() != systemSetting->auditTrailEnabled)
        || (ui->forceTrafficEncryptionCheckBox->isChecked()
            != systemSetting->trafficEncryptionForced)
        || (ui->useHttpsOnlyCamerasCheckBox->isChecked() != systemSetting->useHttpsOnlyForCameras)
        || (ui->forceVideoTrafficEncryptionCheckBox->isChecked()
            != systemSetting->videoTrafficEncryptionForced)
        || (m_watermarkSettings != systemSetting->watermarkSettings)
        || (calculateSessionLimit() != systemSetting->sessionLimitS)
        || (ui->archiveEncryptionGroupBox->isChecked() != systemSetting->storageEncryption)
        || m_archivePasswordState == ArchivePasswordState::changed
        || m_archivePasswordState == ArchivePasswordState::failedToSet
        || ui->showServersInTreeCheckBox->isChecked()
            != systemSetting->showServersInTreeForNonAdmins;
}

void SecuritySettingsWidget::resetWarnings()
{
    ui->forceVideoEncryptionWarning->hide();
    ui->useHttpsOnlyCamerasWarning->hide();
}

void SecuritySettingsWidget::setReadOnlyInternal(bool readOnly)
{
    using ::setReadOnly;

    setReadOnly(ui->auditTrailCheckBox, readOnly);
    setReadOnly(ui->forceTrafficEncryptionCheckBox, readOnly);
    setReadOnly(ui->useHttpsOnlyCamerasCheckBox, readOnly);
    setReadOnly(ui->forceVideoTrafficEncryptionCheckBox, readOnly);
    setReadOnly(ui->displayWatermarkCheckBox, readOnly);
    setReadOnly(ui->watermarkSettingsButton, readOnly);
    setReadOnly(ui->limitSessionLengthCheckBox, readOnly);
    setReadOnly(ui->limitSessionDetailsWidget, readOnly);
    setReadOnly(ui->archiveEncryptionGroupBox, readOnly);
    setReadOnly(ui->showServersInTreeCheckBox, readOnly);
}

void SecuritySettingsWidget::showEvent(QShowEvent* event)
{
    base_type::showEvent(event);
    loadUserInfoToUi();
}

} // namespace nx::vms::client::desktop
