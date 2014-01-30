#include "camera_management_widget.h"
#include "ui_camera_management_widget.h"

#include <utils/common/warnings.h>

#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>

#include <api/resource_property_adaptor.h>

QnCameraManagementWidget::QnCameraManagementWidget(QWidget *parent):
    QWidget(parent),
    ui(new Ui::CameraManagementWidget)
{
    ui->setupUi(this);

    QnUserResourcePtr admin;
    foreach(const QnUserResourcePtr &user, qnResPool->getResources().filtered<QnUserResource>())
        if(user->getName() == lit("admin"))
            admin = user;
    if(!admin) {
        qnWarning("User 'admin' not found.");
        admin.reset(new QnUserResource()); /* Just to prevent it from crashing. */
    }

    m_autoDiscoveryAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(admin, lit("autoCameraDiscovery"), true, this);
    m_autoSettingsAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(admin, lit("autoCameraSettings"), true, this);
}

QnCameraManagementWidget::~QnCameraManagementWidget() {
    return;
}

void QnCameraManagementWidget::updateFromSettings() {
    ui->autoDiscoveryCheckBox->setChecked(m_autoDiscoveryAdaptor->value());
    ui->autoSettingsCheckBox->setChecked(m_autoSettingsAdaptor->value());
}

void QnCameraManagementWidget::submitToSettings() {
    m_autoDiscoveryAdaptor->setValue(ui->autoDiscoveryCheckBox->isChecked());
    m_autoSettingsAdaptor->setValue(ui->autoSettingsCheckBox->isChecked());
}

