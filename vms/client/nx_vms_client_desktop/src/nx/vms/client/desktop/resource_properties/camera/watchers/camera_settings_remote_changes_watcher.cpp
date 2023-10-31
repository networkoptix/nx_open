// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_settings_remote_changes_watcher.h"

#include <QtCore/QPointer>

#include <core/resource/camera_resource.h>
#include <core/resource/resource_property_key.h>
#include <nx/utils/algorithm/same.h>
#include <nx/utils/scoped_connections.h>

#include "../flux/camera_settings_dialog_state.h"
#include "../flux/camera_settings_dialog_store.h"

namespace nx::vms::client::desktop {

class CameraSettingsRemoteChangesWatcher::Private: public QObject
{
public:
    Private(CameraSettingsDialogStore* store): QObject(), store(store) {}

    void setCameras(const QnVirtualCameraResourceList& value)
    {
        if (cameras == value)
            return;

        connections.reset();
        cameras = value;

        for (const auto& camera: cameras)
        {
            connections << connect(camera.get(), &QnResource::propertyChanged,
                this, &Private::handlePropertyChanged);

            connections << connect(camera.get(), &QnResource::statusChanged,
                this, &Private::handleStatusChanged);

            connections << connect(camera.get(),
                &QnSecurityCamResource::disableDualStreamingChanged,
                this,
                &Private::handleDualStreamingChanged);

            connections << connect(camera.get(),
                &QnSecurityCamResource::motionTypeChanged,
                this,
                &Private::handleMotionTypeChanged);

            connections << connect(camera.get(),
                &QnSecurityCamResource::motionRegionChanged,
                this,
                &Private::handleMotionRegionsChanged);

            connections << connect(camera.get(),
                &QnVirtualCameraResource::compatibleObjectTypesMaybeChanged,
                this,
                &Private::handleCompatibleObjectTypesMaybeChanged);

            connections << connect(camera.get(),
                &QnSecurityCamResource::audioEnabledChanged,
                this,
                &Private::handleAudioEnabledChanged);
        }
    }

private:
    void handleAudioEnabledChanged(const QnResourcePtr& /*resource*/)
    {
        if (store)
            store->handleAudioEnabledChanged(cameras);
    }

    void handleDualStreamingChanged(const QnResourcePtr& /*resource*/)
    {
        if (store)
            store->handleDualStreamingChanged(cameras);
    };

    void handleMotionTypeChanged(const QnResourcePtr& /*resource*/)
    {
        if (store)
            store->handleMotionTypeChanged(cameras);
    };

    void handleMotionRegionsChanged(const QnResourcePtr& /*resource*/)
    {
        if (store)
            store->handleMotionRegionsChanged(cameras);
    };

    void handleCompatibleObjectTypesMaybeChanged(const QnVirtualCameraResourcePtr& /*camera*/)
    {
        if (store)
            store->handleCompatibleObjectTypesMaybeChanged(cameras);
    }

    void handlePropertyChanged(const QnResourcePtr& /*resource*/, const QString& key)
    {
        if (!store)
            return;

        if (key == ResourcePropertyKey::kStreamUrls)
            store->handleStreamUrlsChanged(cameras);
        else if (key == ResourcePropertyKey::kMediaStreams)
            store->handleMediaStreamsChanged(cameras);
        else if (key == ResourcePropertyKey::kMotionStreamKey)
            store->handleMotionStreamChanged(cameras);
        else if (key == ResourcePropertyKey::kForcedMotionDetectionKey)
            store->handleMotionForcedChanged(cameras);
        else if (key == ResourcePropertyKey::kMediaCapabilities)
            store->handleMediaCapabilitiesChanged(cameras);
    }

    void handleStatusChanged(const QnResourcePtr& /*resource*/, Qn::StatusChangeReason /*reason*/)
    {
        if (store)
            store->handleStatusChanged(cameras);
    }

private:
    QPointer<CameraSettingsDialogStore> store;
    QnVirtualCameraResourceList cameras;
    nx::utils::ScopedConnections connections;
};

CameraSettingsRemoteChangesWatcher::CameraSettingsRemoteChangesWatcher(
    CameraSettingsDialogStore* store,
    QObject* parent)
    :
    base_type(parent),
    d(new Private(store))
{
}

CameraSettingsRemoteChangesWatcher::~CameraSettingsRemoteChangesWatcher()
{
}

void CameraSettingsRemoteChangesWatcher::setCameras(const QnVirtualCameraResourceList& value)
{
    d->setCameras(value);
}

} // namespace nx::vms::client::desktop
