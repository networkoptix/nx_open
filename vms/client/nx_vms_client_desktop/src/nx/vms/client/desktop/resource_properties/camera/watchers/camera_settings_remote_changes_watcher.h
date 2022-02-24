// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::desktop {

class CameraSettingsDialogStore;

class NX_VMS_CLIENT_DESKTOP_API CameraSettingsRemoteChangesWatcher: public QObject
{
    using base_type = QObject;

public:
    CameraSettingsRemoteChangesWatcher(
        CameraSettingsDialogStore* store, QObject* parent = nullptr);

    virtual ~CameraSettingsRemoteChangesWatcher() override;

    // Must be called after store->loadCameras().
    void setCameras(const QnVirtualCameraResourceList& value);

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
