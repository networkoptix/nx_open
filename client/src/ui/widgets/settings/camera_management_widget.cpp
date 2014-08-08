#include "camera_management_widget.h"
#include "ui_camera_management_widget.h"

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

#include <api/global_settings.h>

QnCameraManagementWidget::QnCameraManagementWidget(QWidget *parent):
    QnAbstractPreferencesWidget(parent),
    ui(new Ui::CameraManagementWidget)
{
    ui->setupUi(this);

    setHelpTopic(ui->autoDiscoveryCheckBox, Qn::SystemSettings_Server_CameraAutoDiscovery_Help);

    connect(ui->autoDiscoveryCheckBox,    SIGNAL(clicked()),                      this,   SLOT(at_autoDiscoveryCheckBox_clicked()));
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
    if (disabledVendors.contains(lit("all")))
        ui->autoDiscoveryCheckBox->setCheckState(Qt::CheckState::Unchecked);
    else if (disabledVendors.isEmpty())
        ui->autoDiscoveryCheckBox->setCheckState(Qt::CheckState::Checked);
    else
        ui->autoDiscoveryCheckBox->setCheckState(Qt::CheckState::PartiallyChecked);

    ui->autoSettingsCheckBox->setChecked(settings->isCameraSettingsOptimizationEnabled());
}

void QnCameraManagementWidget::submitToSettings() {
    QnGlobalSettings *settings = QnGlobalSettings::instance();
    
    if (ui->autoDiscoveryCheckBox->checkState() == Qt::CheckState::Checked)
        settings->setDisabledVendors(QString());
    else if (ui->autoDiscoveryCheckBox->checkState() == Qt::CheckState::Unchecked)
        settings->setDisabledVendors(lit("all"));

    settings->setCameraSettingsOptimizationEnabled(ui->autoSettingsCheckBox->isChecked());
}

bool QnCameraManagementWidget::hasChanges() const  {
    QnGlobalSettings *settings = QnGlobalSettings::instance();

    if (ui->autoDiscoveryCheckBox->checkState() == Qt::CheckState::Checked
        && !settings->disabledVendors().isEmpty())
         return true;
    
    if (ui->autoDiscoveryCheckBox->checkState() == Qt::CheckState::Unchecked
        && settings->disabledVendors() != lit("all"))
         return true;

    if (settings->isCameraSettingsOptimizationEnabled() != ui->autoSettingsCheckBox->isChecked())
        return true;

    return false;
}

