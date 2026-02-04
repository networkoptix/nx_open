// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/camera_history.h>

namespace nx::vms::client::core {

class SystemContext;

class CameraHistoryPool: public QnCameraHistoryPool
{
    Q_OBJECT

public:
    CameraHistoryPool(SystemContext* systemContext);

protected:
    virtual rest::ServerConnectionPtr connectedServerApi() const override;
};

} // namespace nx::vms::client::core
