#include "camera_settings_readonly_watcher.h"

#include <common/common_module.h>

#include <core/resource/camera_resource.h>

#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>

#include "../redux/camera_settings_dialog_store.h"

namespace nx {
namespace client {
namespace desktop {

CameraSettingsReadOnlyWatcher::CameraSettingsReadOnlyWatcher(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent, InitializationMode::lazy)
{
}

QnVirtualCameraResourceList CameraSettingsReadOnlyWatcher::cameras() const
{
    return m_cameras;
}

void CameraSettingsReadOnlyWatcher::setCameras(const QnVirtualCameraResourceList& value)
{
    if (m_cameras == value)
        return;

    m_cameras = value;
    updateReadOnly();
}

void CameraSettingsReadOnlyWatcher::setStore(CameraSettingsDialogStore* store)
{
    connect(this, &CameraSettingsReadOnlyWatcher::readOnlyChanged, store,
        [store](bool value) { store->setReadOnly(value); });
}

void CameraSettingsReadOnlyWatcher::afterContextInitialized()
{
    connect(context(), &QnWorkbenchContext::userChanged, this,
        &CameraSettingsReadOnlyWatcher::updateReadOnly);

    connect(commonModule(), &QnCommonModule::readOnlyChanged, this,
        &CameraSettingsReadOnlyWatcher::updateReadOnly);
}

void CameraSettingsReadOnlyWatcher::updateReadOnly()
{
    const auto value = calculateReadOnly();
    if (m_readOnly == value)
        return;

    m_readOnly = value;
    emit readOnlyChanged(value);
}

bool CameraSettingsReadOnlyWatcher::calculateReadOnly() const
{
    return commonModule()->isReadOnly()
        || !accessController()->combinedPermissions(m_cameras).testFlag(Qn::WritePermission);
}

} // namespace desktop
} // namespace client
} // namespace nx
