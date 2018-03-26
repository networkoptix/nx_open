#include "camera_settings_readonly_watcher.h"

#include <common/common_module.h>

#include <core/resource/camera_resource.h>

#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>

namespace nx {
namespace client {
namespace desktop {

CameraSettingsReadonlyWatcher::CameraSettingsReadonlyWatcher(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent, InitializationMode::lazy)
{
}

QnVirtualCameraResourceList CameraSettingsReadonlyWatcher::cameras() const
{
    return m_cameras;
}

void CameraSettingsReadonlyWatcher::setCameras(const QnVirtualCameraResourceList& value)
{
    if (m_cameras == value)
        return;

    m_cameras = value;
    updateReadOnly();
}

bool CameraSettingsReadonlyWatcher::isReadOnly() const
{
    return m_readOnly;
}

void CameraSettingsReadonlyWatcher::afterContextInitialized()
{
    connect(context(), &QnWorkbenchContext::userChanged, this,
        &CameraSettingsReadonlyWatcher::updateReadOnly);

    connect(commonModule(), &QnCommonModule::readOnlyChanged, this,
        &CameraSettingsReadonlyWatcher::updateReadOnly);
}

void CameraSettingsReadonlyWatcher::updateReadOnly()
{
    const auto value = calculateReadOnly();
    if (m_readOnly == value)
        return;

    m_readOnly = value;
    emit readOnlyChanged(value);
}

bool CameraSettingsReadonlyWatcher::calculateReadOnly() const
{
    return commonModule()->isReadOnly()
        || !accessController()->combinedPermissions(m_cameras).testFlag(Qn::WritePermission);
}

} // namespace desktop
} // namespace client
} // namespace nx
