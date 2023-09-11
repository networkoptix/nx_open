// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/client_camera.h>
#include <core/resource/resource_fwd.h>

namespace nx::vms::client::desktop::test {

class ClientCameraResourceStub: public QnClientCameraResource
{
    using base_type = QnClientCameraResource;
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
