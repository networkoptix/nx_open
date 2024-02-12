// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_event.h"

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/metatypes.h>
#include <nx/vms/api/types/resource_types.h>
#include <nx/vms/common/system_context.h>

#include "../utils/event_details.h"
#include "../utils/type.h"

namespace nx::vms::rules {

CameraEvent::CameraEvent(std::chrono::microseconds timestamp, State state, nx::Uuid deviceId):
    BasicEvent(timestamp, state),
    m_cameraId(deviceId)
{
}

QString CameraEvent::resourceKey() const
{
    return m_cameraId.toSimpleString();
}

QVariantMap CameraEvent::details(common::SystemContext* context) const
{
    auto result = BasicEvent::details(context);

    result.insert(utils::kHasScreenshotDetailName, true);
    result.insert(utils::kScreenshotLifetimeDetailName, QVariant::fromValue(utils::kScreenshotLifetime));
    result.insert(utils::kSourceStatusDetailName, QVariant::fromValue(sourceStatus(context)));

    return result;
}

nx::vms::api::ResourceStatus CameraEvent::sourceStatus(common::SystemContext* context) const
{
    const auto camera =
        context->resourcePool()->getResourceById<QnVirtualCameraResource>(cameraId());

    if (!camera)
        return api::ResourceStatus::undefined;

    return camera->getStatus();
}

} // namespace nx::vms::rules
