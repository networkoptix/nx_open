// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/vms/client/core/resource/camera_resource.h>

namespace nx::vms::client::desktop::test {

class ClientCameraResourceStub: public core::CameraResource
{
    using base_type = core::CameraResource;
    using StreamIndex = nx::vms::api::StreamIndex;

public:
    ClientCameraResourceStub();

    virtual bool setProperty(
        const QString& key,
        const QString& value,
        bool markDirty = true) override;
};

using ClientCameraResourceStubPtr = QnSharedResourcePointer<ClientCameraResourceStub>;
using StubClientCameraResourceList = QnSharedResourcePointerList<ClientCameraResourceStub>;

} // namespace nx::vms::client::desktop::test
