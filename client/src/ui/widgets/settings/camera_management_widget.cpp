#include "camera_management_widget.h"
#include "ui_camera_management_widget.h"

#include <api/global_settings.h>

#include <core/resource/resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/warning_style.h>

QnCameraManagementWidget::QnCameraManagementWidget(QWidget *parent):
    base_type(parent),
    ui(new Ui::CameraManagementWidget)
{
    ui->setupUi(this);

    setHelpTopic(ui->autoDiscoveryCheckBox, Qn::SystemSettings_Server_CameraAutoDiscovery_Help);

    connect(ui->autoDiscoveryCheckBox,  &QCheckBox::clicked,         this,   &QnCameraManagementWidget::at_autoDiscoveryCheckBox_clicked);
    connect(ui->autoDiscoveryCheckBox,  &QCheckBox::stateChanged,    this,   &QnCameraManagementWidget::updateFailoverWarning);

    auto connectToServer = [this](const QnMediaServerResourcePtr &server) {
        connect(server, &QnMediaServerResource::redundancyChanged, this, &QnCameraManagementWidget::updateFailoverWarning);
    };

    connect(qnResPool, &QnResourcePool::resourceAdded,  this, [this, connectToServer] (const QnResourcePtr &resource) {
        QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
        if (!server)
            return;
        connectToServer(server);
        updateFailoverWarning();
    });

    foreach (const QnMediaServerResourcePtr &server, qnResPool->getResources<QnMediaServerResource>()) 
        connectToServer(server);

    setWarningStyle(ui->failoverWarningLabel);
    updateFailoverWarning();
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


void QnCameraManagementWidget::updateFailoverWarning() {
    bool hasFailover = false;
    foreach (const QnMediaServerResourcePtr &server, qnResPool->getResources<QnMediaServerResource>()) {
        if (!server->isRedundancy())
            continue;
        hasFailover = true;
        break;
    };
    ui->failoverWarningLabel->setVisible(hasFailover && ui->autoDiscoveryCheckBox->checkState() == Qt::Unchecked);
}


void QnCameraManagementWidget::updateFromSettings() {
    QnGlobalSettings *settings = QnGlobalSettings::instance();

    QSet<QString> disabledVendors = settings->disabledVendorsSet();
    if (disabledVendors.contains(lit("all")) || disabledVendors.contains(lit("all=partial")))
        ui->autoDiscoveryCheckBox->setCheckState(Qt::CheckState::Unchecked);
    else if (disabledVendors.isEmpty())
        ui->autoDiscoveryCheckBox->setCheckState(Qt::CheckState::Checked);
    else
        ui->autoDiscoveryCheckBox->setCheckState(Qt::CheckState::PartiallyChecked);

    ui->autoSettingsCheckBox->setChecked(settings->isCameraSettingsOptimizationEnabled());
    updateFailoverWarning();
}

void QnCameraManagementWidget::submitToSettings() {
    QnGlobalSettings *settings = QnGlobalSettings::instance();
    
    if (ui->autoDiscoveryCheckBox->checkState() == Qt::CheckState::Checked)
        settings->setDisabledVendors(QString());
    else if (ui->autoDiscoveryCheckBox->checkState() == Qt::CheckState::Unchecked)
        settings->setDisabledVendors(lit("all=partial"));

    settings->setCameraSettingsOptimizationEnabled(ui->autoSettingsCheckBox->isChecked());
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

    return false;
}

