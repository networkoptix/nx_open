// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::desktop {

class CameraSettingsDialogStore;

class CameraSettingsPtzCapabilitiesWatcher: public QObject
{
    using base_type = QObject;

public:
    CameraSettingsPtzCapabilitiesWatcher(
        CameraSettingsDialogStore* store,
        QObject* parent = nullptr);

    virtual ~CameraSettingsPtzCapabilitiesWatcher() override;

    void setCameras(const QnVirtualCameraResourceSet& value);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // nx::vms::client::desktop
