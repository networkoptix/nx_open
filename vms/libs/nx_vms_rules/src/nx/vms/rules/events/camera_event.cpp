// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_event.h"

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/metatypes.h>
#include <nx/vms/common/system_context.h>

#include "../utils/event_details.h"
#include "../utils/string_helper.h"

namespace nx::vms::rules {

CameraEvent::CameraEvent(std::chrono::microseconds timestamp, State state, QnUuid id):
    BasicEvent(timestamp, state),
    m_source(id)
{
}

QnUuid CameraEvent::source() const
{
    return m_source;
}

void CameraEvent::setSource(QnUuid id)
{
    m_source = id;
}

QVariantMap CameraEvent::details(common::SystemContext* context) const
{
    auto result = BasicEvent::details(context);

    utils::insertIfValid(result, utils::kSourceIdDetailName, QVariant::fromValue(m_source));
    result.insert(utils::kHasScreenshotDetailName, true);
    result.insert(utils::kScreenshotLifetimeDetailName, QVariant::fromValue(utils::kScreenshotLifetime));
    result.insert(utils::kSourceStatusDetailName, QVariant::fromValue(sourceStatus(context)));

    return result;
}

vms::api::ResourceStatus CameraEvent::sourceStatus(common::SystemContext* context) const
{
    const auto camera =
        context->resourcePool()->getResourceById<QnVirtualCameraResource>(m_source);

    if (!camera)
        return api::ResourceStatus::undefined;

    return camera->getStatus();
}

} // namespace nx::vms::rules
