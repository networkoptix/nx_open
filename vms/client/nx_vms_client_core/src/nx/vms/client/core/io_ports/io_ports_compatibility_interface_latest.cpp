// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "io_ports_compatibility_interface_latest.h"

#include <core/resource/camera_resource.h>
#include <nx/vms/api/data/device_data.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/core/utils/log_strings_format.h>

namespace nx::vms::client::core {

IoPortsCompatibilityInterface_latest::IoPortsCompatibilityInterface_latest(
    SystemContext* systemContext,
    QObject* parent)
    :
    IoPortsCompatibilityInterface_5_1(systemContext, parent)
{
}

IoPortsCompatibilityInterface_latest::~IoPortsCompatibilityInterface_latest()
{
}

bool IoPortsCompatibilityInterface_latest::setIoPortState(
    const nx::vms::common::SessionTokenHelperPtr& tokenHelper,
    const QnVirtualCameraResourcePtr& camera,
    const QString& cameraOutputId,
    bool isActive,
    const std::chrono::milliseconds& autoResetTimeout,
    std::optional<nx::vms::api::ResolutionData> targetLockResolutionData,
    ResultCallback callback)
{
    auto internalCallback =
        [this, cameraOutputId, callback](
            bool success,
            rest::Handle /*handle*/,
            rest::ErrorOrData<QByteArray> response)
        {
            NX_LOG_RESPONSE(this, success, response,
                "Extended camera output %1 operation was unsuccessful.", cameraOutputId);

            if (callback)
                callback(success);
        };

    nx::vms::api::IoPortUpdateRequest portDescription{
        .isActive = isActive,
        .autoResetTimeoutMs = autoResetTimeout,
        .targetLock = targetLockResolutionData
    };

    nx::vms::api::DeviceIoUpdateRequest body{
        camera->getId().toString(),
        {{cameraOutputId, portDescription}}
    };

    if (!systemContext()->connectedServerApi())
        return false;

    return systemContext()->connectedServerApi()->sendRequest<rest::ErrorOrData<QByteArray>>(
        tokenHelper,
        nx::network::http::Method::patch,
        QString("/rest/v4/devices/%1/io").arg(camera->getId().toString()),
        nx::network::rest::Params{},
        nx::reflect::json::serialize(body),
        internalCallback,
        this);
}

} // namespace nx::vms::client::core
