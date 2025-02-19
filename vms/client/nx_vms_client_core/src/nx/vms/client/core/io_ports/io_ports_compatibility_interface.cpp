// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "io_ports_compatibility_interface.h"

#include <ranges>

#include <core/resource/camera_resource.h>
#include <nx/vms/client/core/system_context.h>

namespace {

QString cameraOutputIdString(
    const QnVirtualCameraResourcePtr& camera,
    nx::vms::api::ExtendedCameraOutput cameraOutput)
{
    const auto portDescriptions = camera->ioPortDescriptions();
    const auto outputName = QString::fromStdString(nx::reflect::toString(cameraOutput));
    const auto portIter = std::ranges::find_if(portDescriptions,
        [outputName](const QnIOPortData& portData) { return portData.outputName == outputName; });

    if (portIter == portDescriptions.end())
        return "";

    return portIter->id;
}

} // namespace

namespace nx::vms::client::core {

IoPortsCompatibilityInterface::IoPortsCompatibilityInterface(
    SystemContext* systemContext,
    QObject* parent)
    :
    QObject(parent),
    SystemContextAware(systemContext)
{
}

IoPortsCompatibilityInterface::~IoPortsCompatibilityInterface()
{
}

bool IoPortsCompatibilityInterface::setIoPortState(
    const nx::vms::common::SessionTokenHelperPtr& tokenHelper,
    const QnVirtualCameraResourcePtr& camera,
    nx::vms::api::ExtendedCameraOutput cameraOutput,
    bool isActive,
    const std::chrono::milliseconds& autoResetTimeout,
    std::optional<nx::vms::api::ResolutionData> targetLockResolutionData,
    ResultCallback callback)
{
    const QString cameraOutputId = cameraOutputIdString(camera, cameraOutput);
    if (cameraOutputId.isEmpty())
    {
        NX_WARNING(this, "Camera does not have the IO port %1",
            nx::reflect::toString(cameraOutput));
        return false;
    }

    return setIoPortState(
        tokenHelper,
        camera,
        cameraOutputId,
        isActive,
        autoResetTimeout,
        targetLockResolutionData,
        callback);
}

} // namespace nx::vms::client::core
