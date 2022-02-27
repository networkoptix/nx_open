// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_settings_ptz_capabilities_watcher.h"

#include <QtCore/QPointer>

#include <core/resource/param.h>
#include <core/resource/camera_resource.h>
#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/desktop/resource_properties/camera/flux/camera_settings_dialog_store.h>

namespace nx::vms::client::desktop {

struct CameraSettingsPtzCapabilitiesWatcher::Private: QObject
{
    QPointer<CameraSettingsDialogStore> store;
    QnVirtualCameraResourceSet cameras;
    nx::utils::ScopedConnections connections;

    Private(CameraSettingsDialogStore* store);
    void handlePropertyChanged(const QnResourcePtr& resource, const QString& key) const;
    void handlePtzCapabilitiesChanged(const QnVirtualCameraResourcePtr& resource) const;
};

CameraSettingsPtzCapabilitiesWatcher::Private::Private(CameraSettingsDialogStore* store):
    store(store)
{
}

void CameraSettingsPtzCapabilitiesWatcher::Private::handlePropertyChanged(
    const QnResourcePtr& /*resource*/,
    const QString& key) const
{
    if (store && key == ResourcePropertyKey::kPtzCapabilitiesUserIsAllowedToModify)
        store->updatePtzSettings(cameras.toList());
}

void CameraSettingsPtzCapabilitiesWatcher::Private::handlePtzCapabilitiesChanged(
    const QnVirtualCameraResourcePtr& /*resource*/) const
{
    if (store)
        store->updatePtzSettings(cameras.toList());
}

//--------------------------------------------------------------------------------------------------

CameraSettingsPtzCapabilitiesWatcher::CameraSettingsPtzCapabilitiesWatcher(
    CameraSettingsDialogStore* store,
    QObject* parent)
    :
    base_type(parent),
    d(new Private(store))
{
}

CameraSettingsPtzCapabilitiesWatcher::~CameraSettingsPtzCapabilitiesWatcher()
{
}

void CameraSettingsPtzCapabilitiesWatcher::setCameras(const QnVirtualCameraResourceSet& value)
{
    if (d->cameras == value)
        return;

    d->connections.reset();
    d->cameras = value;

    for (const auto& camera: d->cameras)
    {
        d->connections << connect(camera.get(), &QnResource::propertyChanged,
            d.get(), &Private::handlePropertyChanged);
        d->connections << connect(camera.get(), &QnVirtualCameraResource::ptzCapabilitiesChanged,
            d.get(), &Private::handlePtzCapabilitiesChanged);
    }
}

}
