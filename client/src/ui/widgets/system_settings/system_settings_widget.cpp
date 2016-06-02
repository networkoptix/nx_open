#include "system_settings_widget.h"
#include "ui_system_settings_widget.h"

#include <api/global_settings.h>

#include <core/resource/device_dependent_strings.h>

#include <ui/common/checkbox_utils.h>
#include <ui/common/read_only.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/custom_style.h>

QnSystemSettingsWidget::QnSystemSettingsWidget(QWidget *parent):
    base_type(parent),
    ui(new Ui::SystemSettingsWidget)
{
    ui->setupUi(this);

    setHelpTopic(ui->autoDiscoveryCheckBox,     Qn::SystemSettings_Server_CameraAutoDiscovery_Help);
    setHelpTopic(ui->auditTrailCheckBox,        Qn::AuditTrail_Help);
    setHelpTopic(ui->statisticsReportCheckBox,  Qn::SystemSettings_General_AnonymousUsage_Help);

    setWarningStyle(ui->settingsWarningLabel);

    QnCheckbox::autoCleanTristate(ui->autoDiscoveryCheckBox);
    connect(ui->autoSettingsCheckBox,   &QCheckBox::clicked,  this,  [this]
    {
        ui->settingsWarningLabel->setVisible(!ui->autoSettingsCheckBox->isChecked());
    });

    connect(ui->autoDiscoveryCheckBox,      &QCheckBox::stateChanged, this, &QnAbstractPreferencesWidget::hasChangesChanged);
    connect(ui->auditTrailCheckBox,         &QCheckBox::stateChanged, this, &QnAbstractPreferencesWidget::hasChangesChanged);
    connect(ui->statisticsReportCheckBox,   &QCheckBox::stateChanged, this, &QnAbstractPreferencesWidget::hasChangesChanged);
    connect(ui->autoSettingsCheckBox,       &QCheckBox::stateChanged, this, &QnAbstractPreferencesWidget::hasChangesChanged);

    retranslateUi();

    /* Let suggest these options are changes so rare, so we can safely drop unsaved changes. */
    connect(qnGlobalSettings, &QnGlobalSettings::disabledVendorsChanged,            this,   &QnSystemSettingsWidget::loadDataToUi);
    connect(qnGlobalSettings, &QnGlobalSettings::serverAutoDiscoveryChanged,        this,   &QnSystemSettingsWidget::loadDataToUi);
    connect(qnGlobalSettings, &QnGlobalSettings::auditTrailEnableChanged,           this,   &QnSystemSettingsWidget::loadDataToUi);
    connect(qnGlobalSettings, &QnGlobalSettings::cameraSettingsOptimizationChanged, this,   &QnSystemSettingsWidget::loadDataToUi);
    connect(qnGlobalSettings, &QnGlobalSettings::statisticsAllowedChanged,          this,   &QnSystemSettingsWidget::loadDataToUi);
}

QnSystemSettingsWidget::~QnSystemSettingsWidget()
{}

void QnSystemSettingsWidget::retranslateUi()
{
    ui->autoDiscoveryCheckBox->setText(QnDeviceDependentStrings::getDefaultNameFromSet(
        tr("Enable devices and servers auto discovery"),
        tr("Enable cameras and servers auto discovery")
        ));
    ui->autoSettingsCheckBox->setText(QnDeviceDependentStrings::getDefaultNameFromSet(
        tr("Allow system to optimize devices settings"),
        tr("Allow system to optimize cameras settings")
        ));
}


void QnSystemSettingsWidget::loadDataToUi()
{
    QSet<QString> disabledVendors = qnGlobalSettings->disabledVendorsSet();
    bool discoveryEnabled = (!disabledVendors.contains(lit("all")) && !disabledVendors.contains(lit("all=partial"))) || qnGlobalSettings->isServerAutoDiscoveryEnabled();
    bool discoveryFullEnabled = disabledVendors.isEmpty() && qnGlobalSettings->isServerAutoDiscoveryEnabled();
    ui->autoDiscoveryCheckBox->setCheckState(discoveryEnabled ? (discoveryFullEnabled ? Qt::Checked : Qt::PartiallyChecked)
                                                              : Qt::Unchecked);
    ui->auditTrailCheckBox->setChecked(qnGlobalSettings->isAuditTrailEnabled());

    ui->autoSettingsCheckBox->setChecked(qnGlobalSettings->isCameraSettingsOptimizationEnabled());
    ui->settingsWarningLabel->setVisible(false);

    ui->statisticsReportCheckBox->setChecked(qnGlobalSettings->isStatisticsAllowed());
}

void QnSystemSettingsWidget::applyChanges()
{
    if (!hasChanges())
        return;

    if (ui->autoDiscoveryCheckBox->checkState() == Qt::CheckState::Checked)
    {
        qnGlobalSettings->setDisabledVendors(QString());
        qnGlobalSettings->setServerAutoDiscoveryEnabled(true);
    }
    else if (ui->autoDiscoveryCheckBox->checkState() == Qt::CheckState::Unchecked)
    {
        qnGlobalSettings->setDisabledVendors(lit("all=partial"));
        qnGlobalSettings->setServerAutoDiscoveryEnabled(false);
    }

    qnGlobalSettings->setAuditTrailEnabled(ui->auditTrailCheckBox->isChecked());
    qnGlobalSettings->setCameraSettingsOptimizationEnabled(ui->autoSettingsCheckBox->isChecked());
    qnGlobalSettings->setStatisticsAllowed(ui->statisticsReportCheckBox->isChecked());

    ui->settingsWarningLabel->setVisible(false);
    qnGlobalSettings->synchronizeNow();
}

bool QnSystemSettingsWidget::hasChanges() const
{
    if (isReadOnly())
        return false;

    if (ui->autoDiscoveryCheckBox->checkState() == Qt::CheckState::Checked
        && !qnGlobalSettings->disabledVendors().isEmpty())
        return true;

    if (ui->autoDiscoveryCheckBox->checkState() == Qt::CheckState::Unchecked
        && !qnGlobalSettings->disabledVendors().contains(lit("all")) )  //MUST not overwrite "all" with "all=partial"
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
