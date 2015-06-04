#include "camera_management_widget.h"
#include "ui_camera_management_widget.h"

#include <api/global_settings.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
#include <core/resource/general_attribute_pool.h>
#include <core/resource/media_server_resource.h>
#include <ec2_statictics_reporter.h>

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/warning_style.h>

QnCameraManagementWidget::QnCameraManagementWidget(QWidget *parent):
    QnAbstractPreferencesWidget(parent),
    ui(new Ui::CameraManagementWidget)
{
    ui->setupUi(this);

    setHelpTopic(ui->autoDiscoveryCheckBox, Qn::SystemSettings_Server_CameraAutoDiscovery_Help);
    setWarningStyle(ui->settingsWarningLabel);

    connect(ui->autoDiscoveryCheckBox,  &QCheckBox::clicked,  this,  &QnCameraManagementWidget::at_autoDiscoveryCheckBox_clicked);
    connect(ui->autoSettingsCheckBox,   &QCheckBox::clicked,  this,  [this]{
        ui->settingsWarningLabel->setVisible(!ui->autoSettingsCheckBox->isChecked());
    });
}

QnCameraManagementWidget::~QnCameraManagementWidget() {
    return;
}

void QnCameraManagementWidget::at_autoDiscoveryCheckBox_clicked() {
    Qt::CheckState state = ui->autoDiscoveryCheckBox->checkState();

    ui->autoDiscoveryCheckBox->setTristate(false);
    if (state == Qt::PartiallyChecked)
        ui->autoDiscoveryCheckBox->setCheckState(Qt::Checked);
}

void QnCameraManagementWidget::updateFromSettings() {
    QnGlobalSettings *settings = QnGlobalSettings::instance();

    QSet<QString> disabledVendors = settings->disabledVendorsSet();
    bool discoveryEnabled = (!disabledVendors.contains(lit("all")) && !disabledVendors.contains(lit("all=partial"))) || settings->isServerAutoDiscoveryEnabled();
    bool discoveryFullEnabled = disabledVendors.isEmpty() && settings->isServerAutoDiscoveryEnabled();
    ui->autoDiscoveryCheckBox->setCheckState(discoveryEnabled ? (discoveryFullEnabled ? Qt::Checked : Qt::PartiallyChecked)
                                                              : Qt::Unchecked);

    ui->autoSettingsCheckBox->setChecked(settings->isCameraSettingsOptimizationEnabled());
    ui->settingsWarningLabel->setVisible(false);

    ui->statisticsReportCheckBox->setChecked(ec2::Ec2StaticticsReporter::isAllowed(qnResPool->getResources<QnMediaServerResource>()));
}

void QnCameraManagementWidget::submitToSettings() {
    QnGlobalSettings *settings = QnGlobalSettings::instance();
    
    if (ui->autoDiscoveryCheckBox->checkState() == Qt::CheckState::Checked) {
        settings->setDisabledVendors(QString());
        settings->setServerAutoDiscoveryEnabled(true);
    } else if (ui->autoDiscoveryCheckBox->checkState() == Qt::CheckState::Unchecked) {
        settings->setDisabledVendors(lit("all=partial"));
        settings->setServerAutoDiscoveryEnabled(false);
    }

    settings->setCameraSettingsOptimizationEnabled(ui->autoSettingsCheckBox->isChecked());
    ui->settingsWarningLabel->setVisible(false);

    const auto servers = qnResPool->getResources<QnMediaServerResource>();
    bool statisticsReportAllowed = ec2::Ec2StaticticsReporter::isAllowed(servers);
    if (!ec2::Ec2StaticticsReporter::isDefined(servers)
        || ui->statisticsReportCheckBox->isChecked() != statisticsReportAllowed) {
        ec2::Ec2StaticticsReporter::setAllowed(servers, ui->statisticsReportCheckBox->isChecked());
        propertyDictionary->saveParamsAsync(idListFromResList(servers));
    }
}

bool QnCameraManagementWidget::hasChanges() const  {
    QnGlobalSettings *settings = QnGlobalSettings::instance();

    if (ui->autoDiscoveryCheckBox->checkState() == Qt::CheckState::Checked
        && !settings->disabledVendors().isEmpty())
        return true;
    
    if (ui->autoDiscoveryCheckBox->checkState() == Qt::CheckState::Unchecked
        && !settings->disabledVendors().contains(lit("all")) )  //MUST not overwrite "all" with "all=partial"
        return true;

    if (settings->isCameraSettingsOptimizationEnabled() != ui->autoSettingsCheckBox->isChecked())
        return true;

    const auto servers = qnResPool->getResources<QnMediaServerResource>();
    bool statisticsReportAllowed = ec2::Ec2StaticticsReporter::isAllowed(servers);
    if (ui->statisticsReportCheckBox->isChecked() != statisticsReportAllowed)
        return false;

    return false;
}

