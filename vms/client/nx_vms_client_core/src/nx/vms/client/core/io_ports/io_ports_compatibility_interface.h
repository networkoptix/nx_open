// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <api/model/api_ioport_data.h>
#include <api/server_rest_connection.h>
#include <nx/vms/api/data/resolution_data.h>
#include <nx/vms/client/core/resource/resource_fwd.h>
#include <nx/vms/client/core/system_context_aware.h>
#include <nx/vms/utils/abstract_session_token_helper.h>

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API IoPortsCompatibilityInterface:
    public QObject,
    public SystemContextAware
{
public:
    using ResultCallback = std::function<void(bool success)>;

public:
    IoPortsCompatibilityInterface(SystemContext* systemContext, QObject* parent = nullptr);
    virtual ~IoPortsCompatibilityInterface();

public:
    virtual bool setIoPortState(
        const nx::vms::common::SessionTokenHelperPtr& tokenHelper,
        const QnVirtualCameraResourcePtr& camera,
        const QString& cameraOutputId,
        bool isActive,
        const std::chrono::milliseconds& autoResetTimeout = {},
        std::optional<nx::vms::api::ResolutionData> targetLockResolutionData = {},
        ResultCallback callback = {}) = 0;

    bool setIoPortState(
        const nx::vms::common::SessionTokenHelperPtr& tokenHelper,
        const QnVirtualCameraResourcePtr& camera,
        nx::vms::api::ExtendedCameraOutput cameraOutput,
        bool isActive,
        const std::chrono::milliseconds& autoResetTimeout = {},
        std::optional<nx::vms::api::ResolutionData> targetLockResolutionData = {},
        ResultCallback callback = {});
};

} // namespace nx::vms::client::core
