// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "virtual_camera_settings_state_watcher.h"

#include <chrono>

#include <QtCore/QTimer>

#include <client/client_module.h>
#include <core/resource/camera_resource.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/utils/virtual_camera_manager.h>

#include "../flux/camera_settings_dialog_store.h"
#include "../flux/camera_settings_dialog_state.h"

namespace nx::vms::client::desktop {

namespace {

using namespace std::chrono;
using namespace std::chrono_literals;

static constexpr milliseconds kStatePollPeriod = 2s;

} // namespace

VirtualCameraSettingsStateWatcher::VirtualCameraSettingsStateWatcher(
    CameraSettingsDialogStore* store,
    QObject* parent)
    :
    base_type(parent)
{
    NX_ASSERT(store);

    auto remoteStatePollTimer = new QTimer(this);
    remoteStatePollTimer->start(kStatePollPeriod.count());
    connect(remoteStatePollTimer, &QTimer::timeout,
        this, &VirtualCameraSettingsStateWatcher::updateState);

    connect(qnClientModule->virtualCameraManager(), &VirtualCameraManager::stateChanged, this,
        [this](const VirtualCameraState& virtualCameraState)
        {
            if (m_camera && m_camera->getId() == virtualCameraState.cameraId)
                emit virtualCameraStateChanged(virtualCameraState, {});
        });

    connect(this, &VirtualCameraSettingsStateWatcher::virtualCameraStateChanged,
        store, &CameraSettingsDialogStore::setSingleVirtualCameraState);
}

void VirtualCameraSettingsStateWatcher::setCameras(const QnVirtualCameraResourceList& cameras)
{
    const auto camera = cameras.size() == 1 && cameras.front()->hasFlags(Qn::virtual_camera)
        ? cameras.front()
        : QnVirtualCameraResourcePtr();

    if (m_camera == camera)
        return;

    m_camera = camera;
    updateState();

    const auto virtualCameraState = qnClientModule->virtualCameraManager()->state(m_camera);
    emit virtualCameraStateChanged(virtualCameraState, {});
}

void VirtualCameraSettingsStateWatcher::updateState()
{
    if (!m_camera)
        return;

    // Remote state is not updated automatically, so it must be polled if not locked locally.

    const auto status = qnClientModule->virtualCameraManager()->state(m_camera).status;
    if (status == VirtualCameraState::Unlocked || status == VirtualCameraState::LockedByOtherClient)
        qnClientModule->virtualCameraManager()->updateState(m_camera);
}

} // namespace nx::vms::client::desktop
