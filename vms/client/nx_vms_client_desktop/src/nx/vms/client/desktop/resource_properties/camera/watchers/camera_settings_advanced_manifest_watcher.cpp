// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_settings_advanced_manifest_watcher.h"

#include <QtCore/QPointer>

#include <core/resource/camera_resource.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/desktop/server_runtime_events/server_runtime_event_connector.h>

#include "../camera_advanced_parameters_manifest_manager.h"
#include "../flux/camera_settings_dialog_store.h"

namespace nx::vms::client::desktop {

struct CameraSettingsAdvancedManifestWatcher::Private
{
    QPointer<CameraAdvancedParametersManifestManager> manager;
    QPointer<CameraSettingsDialogStore> store;
    QnVirtualCameraResourcePtr camera;
    bool isOnline = false;
};

CameraSettingsAdvancedManifestWatcher::CameraSettingsAdvancedManifestWatcher(
    CameraAdvancedParametersManifestManager* manager,
    ServerRuntimeEventConnector* serverRuntimeEventConnector,
    CameraSettingsDialogStore* store,
    QObject* parent)
    :
    base_type(parent),
    d(new Private())
{
    NX_ASSERT(manager);
    d->manager = manager;

    NX_ASSERT(store);
    d->store = store;

    connect(manager, &CameraAdvancedParametersManifestManager::manifestLoaded,
        this,
        [this](const QnVirtualCameraResourcePtr& camera, const QnCameraAdvancedParams& manifest)
        {
            if (d->camera == camera)
                d->store->setAdvancedSettingsManifest(manifest);
        });

    connect(
        serverRuntimeEventConnector,
        &ServerRuntimeEventConnector::deviceAdvancedSettingsManifestChanged,
        this,
        [this](const std::set<QnUuid>& deviceIds)
        {
            if (!d->camera)
                return;

            if (deviceIds.find(d->camera->getId()) != deviceIds.cend())
                d->manager->loadManifestAsync(d->camera);
        });
}

CameraSettingsAdvancedManifestWatcher::~CameraSettingsAdvancedManifestWatcher()
{
}

void CameraSettingsAdvancedManifestWatcher::selectCamera(const QnVirtualCameraResourcePtr& camera)
{
    if (d->camera == camera)
        return;

    if (d->camera)
        d->camera->disconnect(this);

    d->camera = camera;

    if (camera)
    {
        d->isOnline = camera->isOnline();

        connect(camera.get(), &QnResource::statusChanged, this,
            [this](const QnResourcePtr& resource, Qn::StatusChangeReason /*reason*/)
            {
                const bool isOnline = resource->isOnline();
                d->store->setSingleCameraIsOnline(isOnline);

                const bool cameraWasReinitialized = (isOnline && !d->isOnline);
                if (cameraWasReinitialized)
                    d->manager->loadManifestAsync(d->camera);
                d->isOnline = isOnline;
            });

        // Forcefully always reload manifest as it may have been changed on the server side while
        // we did not watch the current camera. If it will be the same, nothing will change on the
        // ui side.
        d->manager->loadManifestAsync(camera);
    }
}

} // namespace nx::vms::client::desktop
