#include "camera_settings_wearable_state_watcher.h"
#include "../redux/camera_settings_dialog_store.h"
#include "../redux/camera_settings_dialog_state.h"

#include <chrono>

#include <QtCore/QTimer>

#include <client/client_module.h>
#include <core/resource/camera_resource.h>

#include <nx/client/desktop/utils/wearable_manager.h>
#include <nx/utils/log/assert.h>

namespace nx {
namespace client {
namespace desktop {

namespace {

using namespace std::chrono;
using namespace std::chrono_literals;

static constexpr milliseconds kStatePollPeriodMs = 2s;

} // namespace

CameraSettingsWearableStateWatcher::CameraSettingsWearableStateWatcher(
    CameraSettingsDialogStore* store,
    QObject* parent)
    :
    base_type(parent)
{
    NX_ASSERT(store);

    auto timer = new QTimer(this);
    timer->start(kStatePollPeriodMs.count());
    connect(timer, &QTimer::timeout, this, &CameraSettingsWearableStateWatcher::updateState);

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
    const auto camera = cameras.size() == 1 ? cameras.front() : QnVirtualCameraResourcePtr();
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

    const auto status = qnClientModule->wearableManager()->state(m_camera).status;

    if (status == WearableState::Unlocked || status == WearableState::LockedByOtherClient)
        qnClientModule->wearableManager()->updateState(m_camera);
}

} // namespace desktop
} // namespace client
} // namespace nx
