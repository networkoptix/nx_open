#include "system_settings_widget.h"
#include "ui_system_settings_widget.h"

#include <api/global_settings.h>

#include <common/common_module.h>
#include <utils/common/watermark_settings.h>
#include <nx/client/desktop/watermark/watermark_edit_settings.h>

#include <core/resource/device_dependent_strings.h>

#include <ui/common/read_only.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/custom_style.h>
#include <ini.h>

QnSystemSettingsWidget::QnSystemSettingsWidget(QWidget *parent):
    base_type(parent),
    ui(new Ui::SystemSettingsWidget),
    m_watermarkSettings(new QnWatermarkSettings)
{
    ui->setupUi(this);

    setHelpTopic(ui->autoDiscoveryCheckBox,     Qn::SystemSettings_Server_CameraAutoDiscovery_Help);
    setHelpTopic(ui->auditTrailCheckBox,        Qn::AuditTrail_Help);
    setHelpTopic(ui->statisticsReportCheckBox,  Qn::SystemSettings_General_AnonymousUsage_Help);

    ui->statisticsReportHint->addHintLine(tr("Includes information about system, such as cameras models and firmware versions, number of servers, etc."));
    ui->statisticsReportHint->addHintLine(tr("Does not include any personal information and is completely anonymous."));
    setHelpTopic(ui->statisticsReportHint, Qn::SystemSettings_General_AnonymousUsage_Help);

    ui->auditTrailHint->setHint(tr("Tracks and logs all user actions."));
    setHelpTopic(ui->auditTrailHint, Qn::AuditTrail_Help);

    setWarningStyle(ui->settingsWarningLabel);

    connect(ui->autoSettingsCheckBox,   &QCheckBox::clicked,  this,  [this]
    {
        ui->settingsWarningLabel->setVisible(!ui->autoSettingsCheckBox->isChecked());
    });

    connect(ui->watermarkSettingsButton, &QPushButton::pressed, this,
        [this]
        {
            if (nx::client::desktop::editWatermarkSettings(*m_watermarkSettings, this))
                emit hasChangesChanged();
        });
    // This should go before connecting to hasChangesChanged!
    connect(ui->displayWatermarkCheckBox, &QCheckBox::stateChanged, this,
        [this](int state) { m_watermarkSettings->useWatermark = (state == Qt::Checked); });

    connect(ui->autoDiscoveryCheckBox,      &QCheckBox::stateChanged, this, &QnAbstractPreferencesWidget::hasChangesChanged);
    connect(ui->auditTrailCheckBox,         &QCheckBox::stateChanged, this, &QnAbstractPreferencesWidget::hasChangesChanged);
    connect(ui->statisticsReportCheckBox,   &QCheckBox::stateChanged, this, &QnAbstractPreferencesWidget::hasChangesChanged);
    connect(ui->autoSettingsCheckBox,       &QCheckBox::stateChanged, this, &QnAbstractPreferencesWidget::hasChangesChanged);
    connect(ui->displayWatermarkCheckBox,   &QCheckBox::stateChanged, this, &QnAbstractPreferencesWidget::hasChangesChanged);

    retranslateUi();

    /* Let suggest these options are changes so rare, so we can safely drop unsaved changes. */
    connect(qnGlobalSettings, &QnGlobalSettings::autoDiscoveryChanged,              this,   &QnSystemSettingsWidget::loadDataToUi);
    connect(qnGlobalSettings, &QnGlobalSettings::auditTrailEnableChanged,           this,   &QnSystemSettingsWidget::loadDataToUi);
    connect(qnGlobalSettings, &QnGlobalSettings::cameraSettingsOptimizationChanged, this,   &QnSystemSettingsWidget::loadDataToUi);
    connect(qnGlobalSettings, &QnGlobalSettings::statisticsAllowedChanged,          this,   &QnSystemSettingsWidget::loadDataToUi);
    connect(qnGlobalSettings, &QnGlobalSettings::watermarkChanged,                  this,   &QnSystemSettingsWidget::loadDataToUi);

    if (!nx::client::desktop::ini().enableWatermark)
    {
        ui->watermarkSettingsButton->hide();
        ui->displayWatermarkCheckBox->hide();
    }
}

QnSystemSettingsWidget::~QnSystemSettingsWidget()
{
}

void QnSystemSettingsWidget::retranslateUi()
{
    ui->autoDiscoveryCheckBox->setText(QnDeviceDependentStrings::getDefaultNameFromSet(
        resourcePool(),
        tr("Enable devices and servers auto discovery"),
        tr("Enable cameras and servers auto discovery")));

    ui->autoSettingsCheckBox->setText(QnDeviceDependentStrings::getDefaultNameFromSet(
        resourcePool(),
        tr("Allow System to optimize device settings"),
        tr("Allow System to optimize camera settings")));
}

void QnSystemSettingsWidget::loadDataToUi()
{
    ui->autoDiscoveryCheckBox->setChecked(qnGlobalSettings->isAutoDiscoveryEnabled());
    ui->auditTrailCheckBox->setChecked(qnGlobalSettings->isAuditTrailEnabled());
    ui->autoSettingsCheckBox->setChecked(qnGlobalSettings->isCameraSettingsOptimizationEnabled());
    ui->settingsWarningLabel->setVisible(false);

    ui->statisticsReportCheckBox->setChecked(qnGlobalSettings->isStatisticsAllowed());

    *m_watermarkSettings = qnGlobalSettings->watermarkSettings();
    ui->displayWatermarkCheckBox->setChecked(m_watermarkSettings->useWatermark);
}

void QnSystemSettingsWidget::applyChanges()
{
    if (!hasChanges())
        return;

    qnGlobalSettings->setAutoDiscoveryEnabled(ui->autoDiscoveryCheckBox->isChecked());
    qnGlobalSettings->setAuditTrailEnabled(ui->auditTrailCheckBox->isChecked());
    qnGlobalSettings->setCameraSettingsOptimizationEnabled(ui->autoSettingsCheckBox->isChecked());
    qnGlobalSettings->setStatisticsAllowed(ui->statisticsReportCheckBox->isChecked());

    qnGlobalSettings->setWatermarkSettings(*m_watermarkSettings);

    ui->settingsWarningLabel->setVisible(false);
    qnGlobalSettings->synchronizeNow();
}

bool QnSystemSettingsWidget::hasChanges() const
{
    if (isReadOnly())
        return false;

    if (ui->autoDiscoveryCheckBox->isChecked() != qnGlobalSettings->isAutoDiscoveryEnabled())
        return true;

    if (qnGlobalSettings->isCameraSettingsOptimizationEnabled() != ui->autoSettingsCheckBox->isChecked())
        return true;

    /* Always mark as 'has changes' if we have not still decided to allow the statistics. */
    if (!qnGlobalSettings->isStatisticsAllowedDefined())
        return true;

    if (qnGlobalSettings->isStatisticsAllowed() != ui->statisticsReportCheckBox->isChecked())
        return true;

    if (ui->auditTrailCheckBox->isChecked() != qnGlobalSettings->isAuditTrailEnabled())
        return true;

    if (*m_watermarkSettings != qnGlobalSettings->watermarkSettings())
        return true;

    return false;
}

void QnSystemSettingsWidget::setReadOnlyInternal(bool readOnly)
{
    using ::setReadOnly;

    setReadOnly(ui->autoDiscoveryCheckBox, readOnly);
    setReadOnly(ui->auditTrailCheckBox, readOnly);
    setReadOnly(ui->autoSettingsCheckBox, readOnly);
    setReadOnly(ui->statisticsReportCheckBox, readOnly);
}
