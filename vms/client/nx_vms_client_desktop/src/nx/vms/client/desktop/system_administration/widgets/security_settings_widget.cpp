#include "security_settings_widget.h"
#include "ui_security_settings_widget.h"

#include <api/global_settings.h>
#include <common/common_module.h>
#include <ui/common/read_only.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/workaround/widgets_signals_workaround.h>

#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/common/widgets/hint_button.h>
#include <nx/vms/client/desktop/watermark/watermark_edit_settings.h>

namespace nx::vms::client::desktop {

SecuritySettingsWidget::SecuritySettingsWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::SecuritySettingsWidget)
{
    ui->setupUi(this);
    ui->watermarkExplanationLabel->setText(tr(
        "Watermarks will be displayed over live, archive and exported videos for non-admin users"
        " only. You and other administrators will not see them."));

    setHelpTopic(ui->auditTrailCheckBox, Qn::AuditTrail_Help);
    setHelpTopic(ui->auditTrailHint, Qn::AuditTrail_Help);

    ui->auditTrailHint->setHint(tr("Tracks and logs all user actions."));

    setHelpTopic(ui->limitSessionLengthHint, Qn::LaunchingAndClosing_Help);
    ui->limitSessionLengthHint->setHint(
        tr("Users will be automatically logged out if their session exceeds the specified duration."));

    setHelpTopic(ui->videoTrafficEncryptionHint, Qn::SecureConnection_Help);
    ui->videoTrafficEncryptionHint->setHint(tr("Only client-server video traffic can be encrypted"));

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
            ui->watermarkExplanationLabel->setVisible(state == Qt::Checked);
            ui->watermarkSettingsButton->setVisible(state == Qt::Checked);
        };
    connect(ui->displayWatermarkCheckBox, &QCheckBox::stateChanged,
        this, updateWatermarkState);
    // Need to sync checkbox to button, loadDataToUi() will do the rest.
    updateWatermarkState(ui->displayWatermarkCheckBox->checkState());

    ui->limitSessionUnitsComboBox->addItem(tr("minutes"), 1);
    ui->limitSessionUnitsComboBox->addItem(tr("hours"), 60);
    ui->limitSessionUnitsComboBox->setCurrentIndex(1);

    const auto updateLimitSessionControls =
        [this](bool enabled) { ui->limitSessionDetailsWidget->setVisible(enabled); };

    connect(ui->limitSessionLengthCheckBox, &QCheckBox::toggled,
        this, updateLimitSessionControls);
    updateLimitSessionControls(ui->limitSessionLengthCheckBox->isChecked());

    // All hasChangesChanged connections should go after initial control updates.
    connect(ui->auditTrailCheckBox, &QCheckBox::stateChanged, this,
        &QnAbstractPreferencesWidget::hasChangesChanged);

    connect(ui->forceTrafficEncryptionCheckBox, &QCheckBox::stateChanged, this,
        &QnAbstractPreferencesWidget::hasChangesChanged);
    connect(ui->forceVideoTrafficEncryptionCheckBox, &QCheckBox::stateChanged, this,
        &QnAbstractPreferencesWidget::hasChangesChanged);

    connect(ui->forceTrafficEncryptionCheckBox, &QCheckBox::clicked, this,
        &SecuritySettingsWidget::at_forceTrafficEncryptionCheckBoxClicked);
    connect(ui->forceVideoTrafficEncryptionCheckBox, &QCheckBox::clicked, this,
        &SecuritySettingsWidget::forceVideoTrafficEncryptionChanged);

    connect(ui->displayWatermarkCheckBox, &QCheckBox::stateChanged, this,
        &QnAbstractPreferencesWidget::hasChangesChanged);

    connect(ui->limitSessionLengthCheckBox, &QCheckBox::clicked, this,
        &QnAbstractPreferencesWidget::hasChangesChanged);
    connect(ui->limitSessionValueSpinBox, QnSpinboxIntValueChanged, this,
        &QnAbstractPreferencesWidget::hasChangesChanged);
    connect(ui->limitSessionUnitsComboBox, QnComboboxCurrentIndexChanged, this,
        &QnAbstractPreferencesWidget::hasChangesChanged);

    /* Let suggest these options are changes so rare, so we can safely drop unsaved changes. */
    connect(qnGlobalSettings, &QnGlobalSettings::auditTrailEnableChanged, this,
        &SecuritySettingsWidget::loadDataToUi);
    connect(qnGlobalSettings, &QnGlobalSettings::trafficEncryptionForcedChanged, this,
        &SecuritySettingsWidget::loadDataToUi);
    connect(qnGlobalSettings, &QnGlobalSettings::videoTrafficEncryptionForcedChanged, this,
        &SecuritySettingsWidget::loadDataToUi);
    connect(qnGlobalSettings, &QnGlobalSettings::watermarkChanged, this,
        &SecuritySettingsWidget::loadDataToUi);
    connect(qnGlobalSettings, &QnGlobalSettings::sessionTimeoutChanged, this,
        &SecuritySettingsWidget::loadDataToUi);
}

SecuritySettingsWidget::~SecuritySettingsWidget()
{
}

std::chrono::minutes SecuritySettingsWidget::calculateSessionTimeout() const
{
    if (!ui->limitSessionLengthCheckBox->isChecked())
        return std::chrono::minutes{0};

    return std::chrono::minutes(ui->limitSessionUnitsComboBox->currentData().toInt()
        * ui->limitSessionValueSpinBox->value());
}

void SecuritySettingsWidget::at_forceTrafficEncryptionCheckBoxClicked(bool value)
{
    if (!value)
    {
        if (ui->forceVideoTrafficEncryptionCheckBox->isChecked())
            emit forceVideoTrafficEncryptionChanged(false);
        ui->forceVideoTrafficEncryptionCheckBox->setChecked(false);
    }
    ui->forceVideoTrafficEncryptionWidget->setEnabled(value);
}

void SecuritySettingsWidget::loadDataToUi()
{
    ui->auditTrailCheckBox->setChecked(qnGlobalSettings->isAuditTrailEnabled());

    ui->forceTrafficEncryptionCheckBox->setChecked(qnGlobalSettings->isTrafficEncriptionForced());
    ui->forceVideoTrafficEncryptionCheckBox->setChecked(
        qnGlobalSettings->isVideoTrafficEncriptionForced());
    ui->forceVideoTrafficEncryptionWidget->setEnabled(
        qnGlobalSettings->isTrafficEncriptionForced());

    m_watermarkSettings = qnGlobalSettings->watermarkSettings();
    ui->displayWatermarkCheckBox->setChecked(m_watermarkSettings.useWatermark);

    const auto sessionTimeoutMinutes = qnGlobalSettings->sessionTimeoutLimit().count();
    const bool hasSessionTimeout = sessionTimeoutMinutes > 0;
    ui->limitSessionLengthCheckBox->setChecked(hasSessionTimeout);
    if (hasSessionTimeout)
    {
        for (int index = ui->limitSessionUnitsComboBox->count() - 1; index >= 0; --index)
        {
            const int minutesInPeriod = ui->limitSessionUnitsComboBox->itemData(index).toInt();
            if (sessionTimeoutMinutes % minutesInPeriod == 0)
            {
                ui->limitSessionUnitsComboBox->setCurrentIndex(index);
                ui->limitSessionValueSpinBox->setValue(sessionTimeoutMinutes / minutesInPeriod);
                break;
            }
        }
    }
}

void SecuritySettingsWidget::applyChanges()
{
    if (!hasChanges())
        return;

    qnGlobalSettings->setAuditTrailEnabled(ui->auditTrailCheckBox->isChecked());
    qnGlobalSettings->setTrafficEncriptionForced(ui->forceTrafficEncryptionCheckBox->isChecked());
    qnGlobalSettings->setVideoTrafficEncryptionForced(
        ui->forceVideoTrafficEncryptionCheckBox->isChecked());

    qnGlobalSettings->setWatermarkSettings(m_watermarkSettings);
    qnGlobalSettings->setSessionTimeoutLimit(calculateSessionTimeout());

    qnGlobalSettings->synchronizeNow();
}

bool SecuritySettingsWidget::hasChanges() const
{
    if (isReadOnly())
        return false;

    return (ui->auditTrailCheckBox->isChecked() != qnGlobalSettings->isAuditTrailEnabled())
        || (ui->forceTrafficEncryptionCheckBox->isChecked()
            != qnGlobalSettings->isTrafficEncriptionForced())
        || (ui->forceVideoTrafficEncryptionCheckBox->isChecked()
            != qnGlobalSettings->isVideoTrafficEncriptionForced())
        || (m_watermarkSettings != qnGlobalSettings->watermarkSettings())
        || (calculateSessionTimeout() != qnGlobalSettings->sessionTimeoutLimit());
}

void SecuritySettingsWidget::setReadOnlyInternal(bool readOnly)
{
    using ::setReadOnly;

    setReadOnly(ui->auditTrailCheckBox, readOnly);
    setReadOnly(ui->forceTrafficEncryptionCheckBox, readOnly);
    setReadOnly(ui->forceVideoTrafficEncryptionCheckBox, readOnly);
    setReadOnly(ui->displayWatermarkCheckBox, readOnly);
    setReadOnly(ui->watermarkSettingsButton, readOnly);
    setReadOnly(ui->limitSessionLengthWidget, readOnly);
}

} // namespace nx::vms::client::desktop
