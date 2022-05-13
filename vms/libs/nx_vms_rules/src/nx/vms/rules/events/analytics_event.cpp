// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_event.h"

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/analytics/taxonomy/abstract_state.h>
#include <nx/vms/common/system_context.h>

#include "../utils/event_details.h"

namespace nx::vms::rules {

QMap<QString, QString> AnalyticsEvent::details(common::SystemContext* context) const
{
    auto result = AnalyticsEngineEvent::details(context);

    if (!result.contains(utils::kCaptionDetailName))
        utils::insertIfNotEmpty(result, utils::kCaptionDetailName, analyticsEventCaption(context));

    return result;
}

QString AnalyticsEvent::analyticsEventCaption(common::SystemContext* context) const
{
    auto eventSource = context->resourcePool()->getResourceById(cameraId());
    const auto camera = eventSource.dynamicCast<QnVirtualCameraResource>();

    const auto eventType = camera && camera->systemContext()
        ? camera->systemContext()->analyticsTaxonomyState()->eventTypeById(m_eventTypeId.toString())
        : nullptr;

    return eventType ? eventType->name() : tr("Analytics Event");
}

FilterManifest AnalyticsEvent::filterManifest()
{
    return {};
}

} // namespace nx::vms::rules
