#include "camera_advanced_settings_widget.h"
#include "ui_camera_advanced_settings_widget.h"

#include <common/static_common_module.h>
#include <common/common_module.h>

#include <core/resource/camera_resource.h>
#include <core/resource/param.h>
#include <core/resource/resource_data.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_data_pool.h>
#include <utils/xml/camera_advanced_param_reader.h>

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
}

CameraAdvancedSettingsWidget::~CameraAdvancedSettingsWidget() = default;

QnVirtualCameraResourcePtr CameraAdvancedSettingsWidget::camera() const
{
    return m_camera;
}

void CameraAdvancedSettingsWidget::setCamera(const QnVirtualCameraResourcePtr &camera)
{
    if (m_camera == camera)
        return;

    if (m_camera)
        m_camera->disconnect(this);

    m_camera = camera;

    if (m_camera)
    {
        connect(m_camera, &QnResource::statusChanged, this,
            &CameraAdvancedSettingsWidget::updatePage);
    }

    ui->cameraAdvancedParamsWidget->setCamera(m_camera);
}

void CameraAdvancedSettingsWidget::updateFromResource()
{
    updatePage();

    const bool isIoModule = m_camera && m_camera->isIOModule();

    ui->noSettingsLabel->setText(isIoModule
        ? tr("This I/O module has no advanced settings")
        : tr("This camera has no advanced settings"));
}


bool CameraAdvancedSettingsWidget::hasManualPage() const
{
    if (!m_camera)
        return false;

    const auto& params = QnCameraAdvancedParamsReader::paramsFromResource(m_camera);
    if (params.groups.empty())
        return false;

     return m_camera->isOnline()
        || ui->cameraAdvancedParamsWidget->hasItemsAvailableInOffline();
}

void CameraAdvancedSettingsWidget::updatePage()
{
    ui->stackedWidget->setCurrentWidget(hasManualPage() ? ui->manualPage : ui->noSettingsPage);
}

void CameraAdvancedSettingsWidget::reloadData()
{
    updatePage();

    if (hasManualPage())
        ui->cameraAdvancedParamsWidget->loadValues();
}

bool CameraAdvancedSettingsWidget::hasChanges() const
{
    return hasManualPage() && ui->cameraAdvancedParamsWidget->hasChanges();
}

void CameraAdvancedSettingsWidget::submitToResource()
{
    if (hasChanges())
        ui->cameraAdvancedParamsWidget->saveValues();
}

} // namespace nx::vms::client::desktop
