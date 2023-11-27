// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "security_settings_widget.h"
#include "ui_security_settings_widget.h"

#include <chrono>

#include <api/server_rest_connection.h>
#include <common/common_module.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/guarded_callback.h>
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
constexpr auto kNoLimitSessionDuration = 0s;

} // namespace

namespace nx::vms::client::desktop {

SecuritySettingsWidget::SecuritySettingsWidget(
    api::SaveableSystemSettings* editableSystemSettings,
    QWidget* parent)
    :
    AbstractSystemSettingsWidget(editableSystemSettings, parent),
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

    ui->forceVideoEncryptionWarning->setText(tr("Encrypting video traffic may significantly increase CPU usage"));
    ui->useHttpsOnlyCamerasWarning->setText(tr("Connection with cameras that do not support HTTPS will be lost"));
    connect(ui->forceVideoTrafficEncryptionCheckBox, &QCheckBox::toggled,
        ui->forceVideoEncryptionWarning, &QWidget::setVisible);
    connect(ui->useHttpsOnlyCamerasCheckBox, &QCheckBox::toggled,
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

    // Let's assume these options are changed so rare, that we can safely drop unsaved changes.
    connect(systemSettings(), &SystemSettings::auditTrailEnableChanged, this,
        &SecuritySettingsWidget::loadDataToUi);
    connect(systemSettings(), &SystemSettings::trafficEncryptionForcedChanged, this,
        &SecuritySettingsWidget::loadDataToUi);
    connect(systemSettings(), &SystemSettings::videoTrafficEncryptionForcedChanged, this,
        &SecuritySettingsWidget::loadDataToUi);
    connect(systemSettings(), &SystemSettings::useHttpsOnlyForCamerasChanged, this,
        &SecuritySettingsWidget::loadDataToUi);
    connect(systemSettings(), &SystemSettings::watermarkChanged, this,
        &SecuritySettingsWidget::loadDataToUi);
    connect(systemSettings(), &SystemSettings::sessionTimeoutChanged, this,
        &SecuritySettingsWidget::loadDataToUi);
    connect(systemSettings(), &SystemSettings::useStorageEncryptionChanged, this,
        &SecuritySettingsWidget::loadDataToUi);
    connect(systemSettings(), &SystemSettings::showServersInTreeForNonAdminsChanged, this,
        &SecuritySettingsWidget::loadDataToUi);
}

SecuritySettingsWidget::~SecuritySettingsWidget()
{
    if (!NX_ASSERT(!isNetworkRequestRunning(), "Requests should already be completed."))
        discardChanges();
}

void SecuritySettingsWidget::updateLimitSessionControls()
 {
     const bool hasSessionLimit = ui->limitSessionLengthCheckBox->isChecked();
     ui->limitSessionDetailsWidget->setEnabled(hasSessionLimit);
     ui->limitSessionAlertLabel->setVisible(!hasSessionLimit);

     const auto limit = calculateSessionLimit();
     ui->limitSessionLengthAlertLabel->setVisible(
         limit != kNoLimitSessionDuration && limit > kSessionLengthAlertLimit);

     emit hasChangesChanged();
 }

std::chrono::seconds SecuritySettingsWidget::calculateSessionLimit() const
{
    if (!ui->limitSessionLengthCheckBox->isChecked())
        return kNoLimitSessionDuration;

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
    ui->auditTrailCheckBox->setChecked(systemSettings()->isAuditTrailEnabled());

    ui->forceTrafficEncryptionCheckBox->setChecked(systemSettings()->isTrafficEncryptionForced());
    ui->useHttpsOnlyCamerasCheckBox->setChecked(systemSettings()->useHttpsOnlyForCameras());
    ui->forceVideoTrafficEncryptionCheckBox->setChecked(
        systemSettings()->isVideoTrafficEncryptionForced());

    m_watermarkSettings = systemSettings()->watermarkSettings();
    ui->displayWatermarkCheckBox->setChecked(m_watermarkSettings.useWatermark);

    const auto sessionTimeoutLimit = systemSettings()->sessionTimeoutLimit();
    ui->limitSessionLengthCheckBox->setChecked(
        sessionTimeoutLimit.value_or(kNoLimitSessionDuration) != kNoLimitSessionDuration);
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

    if (!systemSettings()->isVideoTrafficEncryptionForced())
        ui->forceVideoEncryptionWarning->hide();
    if (!systemSettings()->useHttpsOnlyForCameras())
        ui->useHttpsOnlyCamerasWarning->hide();

    loadEncryptionSettingsToUi();
    loadUserInfoToUi();
    updateLimitSessionControls();

    ui->showServersInTreeCheckBox->setChecked(systemSettings()->showServersInTreeForNonAdmins());
}

void SecuritySettingsWidget::loadEncryptionSettingsToUi()
{
    const bool storageEncryption = systemSettings()->useStorageEncryption();
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

    if (!NX_ASSERT(m_currentRequest == 0, "Request was already sent"))
        return;

    editableSystemSettings->auditTrailEnabled = ui->auditTrailCheckBox->isChecked();
    editableSystemSettings->trafficEncryptionForced =
        ui->forceTrafficEncryptionCheckBox->isChecked();
    editableSystemSettings->useHttpsOnlyForCameras = ui->useHttpsOnlyCamerasCheckBox->isChecked();
    editableSystemSettings->videoTrafficEncryptionForced =
        ui->forceVideoTrafficEncryptionCheckBox->isChecked();
    editableSystemSettings->watermarkSettings = m_watermarkSettings;
    editableSystemSettings->sessionLimitS = calculateSessionLimit();
    editableSystemSettings->storageEncryption = ui->archiveEncryptionGroupBox->isChecked();
    editableSystemSettings->showServersInTreeForNonAdmins =
        ui->showServersInTreeCheckBox->isChecked();

    resetWarnings();

    const auto callback = nx::utils::guarded(this,
        [this](
            bool success,
            rest::Handle requestId,
            const rest::ServerConnection::EmptyResponseType& /*response*/)
        {
            NX_ASSERT(m_currentRequest == requestId || m_currentRequest == 0);
            m_currentRequest = 0;

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
        if (auto api = connectedServerApi(); NX_ASSERT(api, "Connection must be established"))
        {
            m_currentRequest = api->setStorageEncryptionPassword(
                m_archiveEncryptionPasswordDialog->password(),
                /*makeCurrent*/ true,
                /*salt*/ QByteArray(),
                callback,
                thread());
        }
    }
}

void SecuritySettingsWidget::discardChanges()
{
    if (auto api = connectedServerApi(); api && m_currentRequest > 0)
        api->cancelRequest(m_currentRequest);
    m_currentRequest = 0;
}

bool SecuritySettingsWidget::hasChanges() const
{
    if (isReadOnly())
        return false;

    return (ui->auditTrailCheckBox->isChecked() != systemSettings()->isAuditTrailEnabled())
        || (ui->forceTrafficEncryptionCheckBox->isChecked()
            != systemSettings()->isTrafficEncryptionForced())
        || (ui->useHttpsOnlyCamerasCheckBox->isChecked()
            != systemSettings()->useHttpsOnlyForCameras())
        || (ui->forceVideoTrafficEncryptionCheckBox->isChecked()
            != systemSettings()->isVideoTrafficEncryptionForced())
        || (m_watermarkSettings != systemSettings()->watermarkSettings())
        || (calculateSessionLimit() != systemSettings()->sessionTimeoutLimit().value_or(kNoLimitSessionDuration))
        || (ui->archiveEncryptionGroupBox->isChecked() != systemSettings()->useStorageEncryption())
        || m_archivePasswordState == ArchivePasswordState::changed
        || m_archivePasswordState == ArchivePasswordState::failedToSet
        || ui->showServersInTreeCheckBox->isChecked()
            != systemSettings()->showServersInTreeForNonAdmins();
}

bool SecuritySettingsWidget::isNetworkRequestRunning() const
{
    return m_currentRequest != 0;
}

void SecuritySettingsWidget::resetWarnings()
{
    ui->forceVideoEncryptionWarning->hide();
    ui->useHttpsOnlyCamerasWarning->hide();
}

void SecuritySettingsWidget::resetChanges()
{
    if (!hasChanges())
        return;

    discardChanges();
    loadDataToUi();
    emit hasChangesChanged();
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
    AbstractSystemSettingsWidget::showEvent(event);
    loadUserInfoToUi();
}

} // namespace nx::vms::client::desktop
