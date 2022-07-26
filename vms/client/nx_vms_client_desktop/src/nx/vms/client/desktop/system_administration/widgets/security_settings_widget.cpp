// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "security_settings_widget.h"
#include "ui_security_settings_widget.h"

#include <api/server_rest_connection.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/client/desktop/common/dialogs/repeated_password_dialog.h>
#include <nx/vms/client/desktop/common/widgets/hint_button.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/watermark/watermark_edit_settings.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/text/human_readable.h>
#include <ui/common/read_only.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/help/help_handler.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/workaround/widgets_signals_workaround.h>

using namespace nx::vms::common;

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

    setHelpTopic(ui->useHttpsOnlyCamerasCheckBox, Qn::ConnectToCamerasOverOnlyHttps_Help);
    ui->useHttpsOnlyCamerasCheckBox->setHint(ui->useHttpsOnlyCamerasCheckBox->text());

    setHelpTopic(ui->displayWatermarkCheckBox, Qn::UserWatermark_Help);
    ui->displayWatermarkCheckBox->setHint(tr(
        "Watermarks will be displayed over live, archive and exported videos for non-admin users"
        " only. You and other administrators will not see them."));

    setHelpTopic(ui->auditTrailCheckBox, Qn::AuditTrail_Help);
    ui->auditTrailCheckBox->setHint(tr("Tracks and logs all user actions."));

    setHelpTopic(ui->limitSessionLengthCheckBox, Qn::LaunchingAndClosing_Help);
    ui->limitSessionLengthCheckBox->setHint(tr(
        "Local and LDAP users will be automatically logged out if their session exceeds the"
        " specified duration."));

    ui->forceTrafficEncryptionCheckBox->setHint(
        tr("Does not affect the connections established by server."));
    setHelpTopic(ui->forceTrafficEncryptionCheckBox, Qn::ForceSecureConnections_Help);

    setHelpTopic(ui->forceVideoTrafficEncryptionCheckBox, Qn::EnableEncryptedVideoTraffic_Help);
    ui->forceVideoTrafficEncryptionCheckBox->setHint(tr(
        "Enables RTSP traffic encryption."));

    setHelpTopic(ui->archiveEncryptionGroupBox, Qn::EnableArchiveEncryption_Help);
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

    ui->limitSessionUnitsComboBox->addItem(tr("minutes"), 1);
    ui->limitSessionUnitsComboBox->addItem(tr("hours"), 60);
    ui->limitSessionUnitsComboBox->addItem(tr("days"), 24 * 60);
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
        [] { QnHelpHandler::openHelpTopic(Qn::HelpTopic::SessionAndDigestAuth_Help); });
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

    /* Let suggest these options are changes so rare, so we can safely drop unsaved changes. */
    connect(globalSettings(), &SystemSettings::auditTrailEnableChanged, this,
        &SecuritySettingsWidget::loadDataToUi);
    connect(globalSettings(), &SystemSettings::trafficEncryptionForcedChanged, this,
        &SecuritySettingsWidget::loadDataToUi);
    connect(globalSettings(), &SystemSettings::videoTrafficEncryptionForcedChanged, this,
        &SecuritySettingsWidget::loadDataToUi);
    connect(globalSettings(), &SystemSettings::watermarkChanged, this,
        &SecuritySettingsWidget::loadDataToUi);
    connect(globalSettings(), &SystemSettings::sessionTimeoutChanged, this,
        &SecuritySettingsWidget::loadDataToUi);
    connect(globalSettings(), &SystemSettings::useStorageEncryptionChanged, this,
        &SecuritySettingsWidget::loadDataToUi);
    connect(globalSettings(), &SystemSettings::showServersInTreeForNonAdminsChanged, this,
        &SecuritySettingsWidget::loadDataToUi);
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

std::optional<std::chrono::minutes> SecuritySettingsWidget::calculateSessionLimit() const
{
    if (!ui->limitSessionLengthCheckBox->isChecked())
        return std::nullopt;

    return std::chrono::minutes(ui->limitSessionUnitsComboBox->currentData().toInt()
        * ui->limitSessionValueSpinBox->value());
}

void SecuritySettingsWidget::showArchiveEncryptionPasswordDialog(bool viaButton)
{
    m_archiveEncryptionPasswordDialogOpenedViaButton = viaButton;
    m_archiveEncryptionPasswordDialog->clearInputFields();
    m_archiveEncryptionPasswordDialog->show();
}

void SecuritySettingsWidget::loadDataToUi()
{
    ui->auditTrailCheckBox->setChecked(globalSettings()->isAuditTrailEnabled());

    ui->forceTrafficEncryptionCheckBox->setChecked(globalSettings()->isTrafficEncryptionForced());
    ui->useHttpsOnlyCamerasCheckBox->setChecked(globalSettings()->useHttpsOnlyCameras());
    ui->forceVideoTrafficEncryptionCheckBox->setChecked(
        globalSettings()->isVideoTrafficEncryptionForced());

    m_watermarkSettings = globalSettings()->watermarkSettings();
    ui->displayWatermarkCheckBox->setChecked(m_watermarkSettings.useWatermark);

    const auto sessionTimeoutLimit = globalSettings()->sessionTimeoutLimit();
    ui->limitSessionLengthCheckBox->setChecked(sessionTimeoutLimit.has_value());
    if (sessionTimeoutLimit)
    {
        for (int index = ui->limitSessionUnitsComboBox->count() - 1; index >= 0; --index)
        {
            const int minutesInPeriod = ui->limitSessionUnitsComboBox->itemData(index).toInt();
            if (sessionTimeoutLimit->count() % minutesInPeriod == 0)
            {
                ui->limitSessionUnitsComboBox->setCurrentIndex(index);
                ui->limitSessionValueSpinBox->setValue(
                    sessionTimeoutLimit->count() / minutesInPeriod);
                break;
            }
        }
    }

    if (!globalSettings()->isVideoTrafficEncryptionForced())
        ui->forceVideoEncryptionWarning->hide();
    if (!globalSettings()->useHttpsOnlyCameras())
        ui->useHttpsOnlyCamerasWarning->hide();

    loadEncryptionSettingsToUi();
    loadUserInfoToUi();
    updateLimitSessionControls();

    ui->showServersInTreeCheckBox->setChecked(globalSettings()->showServersInTreeForNonAdmins());
}

void SecuritySettingsWidget::loadEncryptionSettingsToUi()
{
    const bool storageEncryption = globalSettings()->useStorageEncryption();
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
    const auto users = commonModule()->resourcePool()->getResources<QnUserResource>();

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

    globalSettings()->setAuditTrailEnabled(ui->auditTrailCheckBox->isChecked());
    globalSettings()->setTrafficEncryptionForced(ui->forceTrafficEncryptionCheckBox->isChecked());
    globalSettings()->setUseHttpsOnlyCameras(ui->useHttpsOnlyCamerasCheckBox->isChecked());
    globalSettings()->setVideoTrafficEncryptionForced(
        ui->forceVideoTrafficEncryptionCheckBox->isChecked());

    globalSettings()->setWatermarkSettings(m_watermarkSettings);
    globalSettings()->setSessionTimeoutLimit(calculateSessionLimit());
    globalSettings()->setUseStorageEncryption(ui->archiveEncryptionGroupBox->isChecked());
    globalSettings()->setShowServersInTreeForNonAdmins(ui->showServersInTreeCheckBox->isChecked());

    globalSettings()->synchronizeNow();
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

    return (ui->auditTrailCheckBox->isChecked() != globalSettings()->isAuditTrailEnabled())
        || (ui->forceTrafficEncryptionCheckBox->isChecked()
            != globalSettings()->isTrafficEncryptionForced())
        || (ui->useHttpsOnlyCamerasCheckBox->isChecked()
            != globalSettings()->useHttpsOnlyCameras())
        || (ui->forceVideoTrafficEncryptionCheckBox->isChecked()
            != globalSettings()->isVideoTrafficEncryptionForced())
        || (m_watermarkSettings != globalSettings()->watermarkSettings())
        || (calculateSessionLimit() != globalSettings()->sessionTimeoutLimit())
        || (ui->archiveEncryptionGroupBox->isChecked()
            != globalSettings()->useStorageEncryption())
        || m_archivePasswordState == ArchivePasswordState::changed
        || m_archivePasswordState == ArchivePasswordState::failedToSet
        || ui->showServersInTreeCheckBox->isChecked()
            != globalSettings()->showServersInTreeForNonAdmins();
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
