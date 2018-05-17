#include "camera_settings_readonly_watcher.h"
#include "../redux/camera_settings_dialog_store.h"

#include <QtCore/QPointer>

#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>

#include <nx/utils/log/assert.h>

namespace nx {
namespace client {
namespace desktop {

CameraSettingsReadOnlyWatcher::CameraSettingsReadOnlyWatcher(
    CameraSettingsDialogStore* store,
    QObject* parent)
    :
    base_type(parent),
    QnWorkbenchContextAware(parent, InitializationMode::lazy)
{
    NX_ASSERT(store);

    connect(this, &CameraSettingsReadOnlyWatcher::readOnlyChanged,
        store, &CameraSettingsDialogStore::setReadOnly);
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
