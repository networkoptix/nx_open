// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_settings_advanced_manifest_watcher.h"

#include <QtCore/QPointer>

#include <api/server_rest_connection.h>
#include <core/resource/camera_resource.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/core/server_runtime_events/server_runtime_event_connector.h>
#include <nx/vms/client/desktop/system_context.h>

#include "../camera_advanced_parameters_manifest_manager.h"
#include "../flux/camera_settings_dialog_store.h"

namespace nx::vms::client::desktop {

struct CameraSettingsAdvancedManifestWatcher::Private
{
    QPointer<CameraAdvancedParametersManifestManager> manager;
    QPointer<CameraSettingsDialogStore> store;
    QnVirtualCameraResourcePtr camera;
    bool isOnline = false;
    rest::Handle currentRequest = 0;

    void cancelCurrentRequest()
    {
        if (currentRequest == 0)
            return;

        auto systemContext = SystemContext::fromResource(camera);
        if (NX_ASSERT(camera, "Request must be cancelled before switching camera") &&
            NX_ASSERT(systemContext))
        {
            if (auto api = systemContext->connectedServerApi())
                api->cancelRequest(currentRequest);
        }
        currentRequest = 0;
    }

    void updateManifest()
    {
        if (!NX_ASSERT(camera))
            return;

        cancelCurrentRequest();
        currentRequest = manager->loadManifestAsync(camera);
    }
};

CameraSettingsAdvancedManifestWatcher::CameraSettingsAdvancedManifestWatcher(
    CameraAdvancedParametersManifestManager* manager,
    core::ServerRuntimeEventConnector* serverRuntimeEventConnector,
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
        [this](
            rest::Handle handle,
            const QnVirtualCameraResourcePtr& camera,
            const QnCameraAdvancedParams& manifest)
        {
            if (d->currentRequest == handle)
                d->currentRequest = 0;

            if (d->camera == camera)
            {
                NX_VERBOSE(this, "Set analytics manifest to the camera settings dialog");
                d->store->setAdvancedSettingsManifest(manifest);
            }
        });

    connect(manager, &CameraAdvancedParametersManifestManager::manifestLoadingFailed,
        this,
        [this](rest::Handle handle, const QnVirtualCameraResourcePtr& camera)
        {
            if (d->currentRequest == handle)
                d->currentRequest = 0;

            // Do not re-request if another request is already running.
            if (d->camera == camera && d->currentRequest == 0)
            {
                NX_VERBOSE(this, "Re-requesting manifest from the server");
                d->updateManifest();
            }
        });

    connect(
        serverRuntimeEventConnector,
        &core::ServerRuntimeEventConnector::deviceAdvancedSettingsManifestChanged,
        this,
        [this](const std::set<QnUuid>& deviceIds)
        {
            if (d->camera && deviceIds.contains(d->camera->getId()))
            {
                NX_VERBOSE(this, "Device manifest was changed for %1", d->camera);
                d->updateManifest();
            }
        });
}

CameraSettingsAdvancedManifestWatcher::~CameraSettingsAdvancedManifestWatcher()
{
}

void CameraSettingsAdvancedManifestWatcher::selectCamera(const QnVirtualCameraResourcePtr& camera)
{
    if (d->camera == camera)
        return;

    d->cancelCurrentRequest();
    if (d->camera)
        d->camera->disconnect(this);

    d->camera = camera;
    NX_VERBOSE(this, "Select camera %1", camera);

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
                {
                    NX_VERBOSE(this, "Camera %1 was reinitialized, update manifest", d->camera);
                    d->updateManifest();
                }
                d->isOnline = isOnline;
            });

        // Forcefully always reload manifest as it may have been changed on the server side while
        // we did not watch the current camera. If it will be the same, nothing will change on the
        // ui side.
        NX_VERBOSE(this, "Forcefully reload camera %1 manifest", d->camera);
        d->updateManifest();
    }
}

} // namespace nx::vms::client::desktop
