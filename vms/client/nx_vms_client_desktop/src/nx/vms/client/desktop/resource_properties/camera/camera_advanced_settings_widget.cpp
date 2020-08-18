#include "camera_advanced_settings_widget.h"
#include "ui_camera_advanced_settings_widget.h"

#include <common/static_common_module.h>
#include <common/common_module.h>

#include <core/resource/camera_resource.h>
#include <core/resource/param.h>
#include <core/resource/resource_data.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_data_pool.h>

namespace nx::vms::client::desktop {

CameraAdvancedSettingsWidget::CameraAdvancedSettingsWidget(QWidget* parent /* = 0*/):
    base_type(parent),
    ui(new Ui::CameraAdvancedSettingsWidget)
{
    ui->setupUi(this);

    connect(ui->cameraAdvancedParamsWidget,
        &CameraAdvancedParamsWidget::hasChangesChanged,
        this,
        &CameraAdvancedSettingsWidget::hasChangesChanged);

    connect(ui->cameraAdvancedParamsWidget,
        &CameraAdvancedParamsWidget::visibilityUpdateRequested,
        this,
        &CameraAdvancedSettingsWidget::visibilityUpdateRequested);
}

CameraAdvancedSettingsWidget::~CameraAdvancedSettingsWidget() = default;

void CameraAdvancedSettingsWidget::setCamera(const QnVirtualCameraResourcePtr &camera)
{
    ui->cameraAdvancedParamsWidget->setCamera(camera);
}

void CameraAdvancedSettingsWidget::reloadData()
{
    if (shouldBeVisible())
        ui->cameraAdvancedParamsWidget->loadValues();
}

bool CameraAdvancedSettingsWidget::hasChanges() const
{
    return shouldBeVisible() && ui->cameraAdvancedParamsWidget->hasChanges();
}

void CameraAdvancedSettingsWidget::submitToResource()
{
    if (hasChanges())
        ui->cameraAdvancedParamsWidget->saveValues();
}

bool CameraAdvancedSettingsWidget::shouldBeVisible() const
{
    return ui->cameraAdvancedParamsWidget->shouldBeVisible();
}

} // namespace nx::vms::client::desktop
