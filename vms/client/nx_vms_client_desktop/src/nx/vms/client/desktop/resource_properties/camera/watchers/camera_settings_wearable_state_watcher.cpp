#include "camera_settings_wearable_state_watcher.h"
#include "../redux/camera_settings_dialog_store.h"
#include "../redux/camera_settings_dialog_state.h"

#include <chrono>

#include <QtCore/QTimer>

#include <client/client_module.h>
#include <core/resource/camera_resource.h>

#include <nx/vms/client/desktop/utils/wearable_manager.h>
#include <nx/utils/log/assert.h>

namespace nx::vms::client::desktop {

namespace {

using namespace std::chrono;
using namespace std::chrono_literals;

static constexpr milliseconds kStatePollPeriod = 2s;

} // namespace

CameraSettingsWearableStateWatcher::CameraSettingsWearableStateWatcher(
    CameraSettingsDialogStore* store,
    QObject* parent)
    :
    base_type(parent)
{
    NX_ASSERT(store);

    auto remoteStatePollTimer = new QTimer(this);
    remoteStatePollTimer->start(kStatePollPeriod.count());
    connect(remoteStatePollTimer, &QTimer::timeout,
        this, &CameraSettingsWearableStateWatcher::updateState);

    connect(qnClientModule->wearableManager(), &WearableManager::stateChanged, this,
        [this](const WearableState& wearableState)
        {
            if (m_camera && m_camera->getId() == wearableState.cameraId)
                emit wearableStateChanged(wearableState, {});
        });

    connect(this, &CameraSettingsWearableStateWatcher::wearableStateChanged,
        store, &CameraSettingsDialogStore::setSingleWearableState);
}

void CameraSettingsWearableStateWatcher::setCameras(const QnVirtualCameraResourceList& cameras)
{
    const auto camera = cameras.size() == 1 && cameras.front()->hasFlags(Qn::wearable_camera)
        ? cameras.front()
        : QnVirtualCameraResourcePtr();

    if (m_camera == camera)
        return;

    m_camera = camera;
    updateState();

    const auto wearableState = qnClientModule->wearableManager()->state(m_camera);
    emit wearableStateChanged(wearableState, {});
}

void CameraSettingsWearableStateWatcher::updateState()
{
    if (!m_camera)
        return;

    // Remote state is not updated automatically, so it must be polled if not locked locally.

    const auto status = qnClientModule->wearableManager()->state(m_camera).status;
    if (status == WearableState::Unlocked || status == WearableState::LockedByOtherClient)
        qnClientModule->wearableManager()->updateState(m_camera);
}

} // namespace nx::vms::client::desktop
