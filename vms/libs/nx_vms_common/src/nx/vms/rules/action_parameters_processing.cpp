// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "action_parameters_processing.h"

#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <nx/analytics/event_type_descriptor_manager.h>
#include <core/resource/camera_resource.h>
#include <nx/vms/event/event_parameters.h>
#include <nx/utils/string.h>

namespace nx::vms::rules {

namespace {

/**
 * Update actual text value with applied substitutions (if any).
 */
void processTextFieldSubstitutions(
    QString& value,
    const EventParameters& data,
    const QnCommonModule* commonModule)
{
    using EventType = nx::vms::api::EventType;
    using Keyword = SubstitutionKeywords::Event;

    std::vector<std::pair<QString, QString>> substitutions;

    if (data.eventType >= EventType::userDefinedEvent)
    {
        substitutions.push_back({Keyword::source, data.resourceName});
        substitutions.push_back({Keyword::caption, data.caption});
        substitutions.push_back({Keyword::description, data.description});
    }
    else if (data.eventType == EventType::analyticsSdkEvent)
    {
        substitutions.push_back({Keyword::cameraId, data.eventResourceId.toString()});
        substitutions.push_back({Keyword::caption, data.caption});
        substitutions.push_back({Keyword::description, data.description});
        substitutions.push_back({Keyword::eventType, data.getAnalyticsEventTypeId()});

        if (auto camera = commonModule->resourcePool()->getResourceById<QnVirtualCameraResource>(
            data.eventResourceId))
        {
            substitutions.push_back({Keyword::cameraName, camera->getUserDefinedName()});
        }
        if (auto descriptor = commonModule->analyticsEventTypeDescriptorManager()->descriptor(
            data.getAnalyticsEventTypeId()))
        {
            substitutions.push_back({Keyword::eventName, descriptor->name});
        }
    }

    value = nx::utils::replaceStrings(value, substitutions, Qt::CaseSensitive);
}

} // namespace

const QString SubstitutionKeywords::Event::source("{event.source}");
const QString SubstitutionKeywords::Event::caption("{event.caption}");
const QString SubstitutionKeywords::Event::description("{event.description}");
const QString SubstitutionKeywords::Event::cameraId("{event.cameraId}");
const QString SubstitutionKeywords::Event::cameraName("{event.cameraName}");
const QString SubstitutionKeywords::Event::eventType("{event.eventType}");
const QString SubstitutionKeywords::Event::eventName("{event.eventName}");

ActionParameters actualActionParameters(
    ActionType actionType,
    const ActionParameters& sourceActionParameters,
    const EventParameters& eventRuntimeParameters,
    const QnCommonModule* commonModule)
{
    ActionParameters result = sourceActionParameters;

    if (actionType == ActionType::execHttpRequestAction)
    {
        processTextFieldSubstitutions(result.text, eventRuntimeParameters, commonModule);
    }
    return result;
}

} // namespace nx::vms::rules
