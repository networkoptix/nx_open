// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "io_ports_compatibility_interface_5_1.h"

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API IoPortsCompatibilityInterface_latest:
    public IoPortsCompatibilityInterface_5_1
{
public:
    IoPortsCompatibilityInterface_latest(SystemContext* systemContext, QObject* parent = nullptr);
    virtual ~IoPortsCompatibilityInterface_latest() override;

public:
    virtual bool setIoPortState(
        const nx::vms::common::SessionTokenHelperPtr& tokenHelper,
        const QnVirtualCameraResourcePtr& camera,
        const QString& cameraOutputId,
        bool isActive,
        const std::chrono::milliseconds& autoResetTimeout = {},
        std::optional<nx::vms::api::ResolutionData> targetLockResolutionData = {},
        ResultCallback callback = {}) override;
};

} // namespace nx::vms::client::core
