// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_log_request_data.h"

#include <api/helpers/camera_id_helper.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/event/events/abstract_event.h>

#include <nx/fusion/model_functions.h>
#include <nx/network/rest/params.h>
#include <nx/reflect/string_conversion.h>
#include <nx/utils/string.h>

namespace {

static const QString kCameraIdParam = "cameraId";
static const QString kDeprecatedPhysicalIdParam = "physicalId";
static const QString kStartPeriodParam = "from";
static const QString kEndPeriodParam = "to";
static const QString kEventTypeParam = "event_type";
static const QString kEventSubtypeParam = "event_subtype";
static const QString kActionTypeParam = "action_type";
static const QString kRuleIdParam = "brule_id";
static const QString kFormatParam = "format";
static const QString kTextParam = "text";
static const QString kEventsOnlyParam = "eventsOnly";

static constexpr int kInvalidStartTime = -1;

} // namespace

void QnEventLogFilterData::loadFromParams(
    QnResourcePool* resourcePool, const nx::network::rest::Params& params)
{
    using namespace nx::vms::api;

    nx::camera_id_helper::findAllCamerasByFlexibleIds(
        resourcePool,
        &cameras,
        params,
        {kCameraIdParam, kDeprecatedPhysicalIdParam});

    const auto startTimeUs = params.value(kStartPeriodParam);
    period.startTimeMs = startTimeUs.isEmpty()
        ? QnTimePeriod::kMinTimeValue
        : nx::utils::parseDateTimeMsec(startTimeUs);

    const auto endTimeUs = params.value(kEndPeriodParam);
    period.durationMs = endTimeUs.isEmpty()
        ? QnTimePeriod::kInfiniteDuration
        : nx::utils::parseDateTimeMsec(endTimeUs) - period.startTimeMs;

    for (const auto& value: params.values(kEventTypeParam))
    {
        eventTypeList.push_back(nx::reflect::fromString<EventType>(
            value.toStdString(), EventType::undefinedEvent));
    }

    eventSubtype = params.value(kEventSubtypeParam);

    actionType = nx::reflect::fromString<nx::vms::api::ActionType>(
        params.value(kActionTypeParam).toStdString(),
        actionType);

    ruleId = QnLexical::deserialized<nx::Uuid>(params.value(kRuleIdParam));
    text = params.value(kTextParam);
    eventsOnly = params.contains(kEventsOnlyParam);
    if (eventsOnly)
        eventsOnly = !(params.value(kEventsOnlyParam) == "false");
}

nx::network::rest::Params QnEventLogFilterData::toParams() const
{
    nx::network::rest::Params result;

    for (const auto& camera: cameras)
        result.insert(kCameraIdParam, QnLexical::serialized(camera->getId()));

    result.insert(kStartPeriodParam, QnLexical::serialized(period.startTimeMs));

    if (!period.isInfinite())
        result.insert(kEndPeriodParam, QnLexical::serialized(period.endTimeMs()));

    for (const auto& eventType: eventTypeList)
    {
        if (eventType != nx::vms::api::EventType::undefinedEvent)
            result.insert(kEventTypeParam, nx::reflect::toString(eventType));
    }

    if (!eventSubtype.isEmpty())
        result.insert(kEventSubtypeParam, eventSubtype);

    if (actionType != nx::vms::api::ActionType::undefinedAction)
        result.insert(kActionTypeParam, nx::reflect::toString(actionType));

    if (!text.isEmpty())
        result.insert(kTextParam, text);

    if (eventsOnly)
        result.insert(kEventsOnlyParam, QString());

    if (!ruleId.isNull())
        result.insert(kRuleIdParam, QnLexical::serialized(ruleId));

    return result;
}

bool QnEventLogFilterData::isValid(QString* errorString) const
{
    auto error =
        [errorString](const QString& text)
        {
            if (errorString)
                *errorString = text;
            return false;
        };

    if (period.startTimeMs == kInvalidStartTime)
        return error(lit("Parameter %1 MUST be specified").arg(kStartPeriodParam));

    for (const auto& eventType: eventTypeList)
    {
        if (eventType != nx::vms::api::EventType::undefinedEvent
            && !nx::vms::event::allEvents(/*includeDeprecated*/ true).contains(eventType)
            && !nx::vms::event::hasChild(eventType))
        {
            return error(lit("Invalid event type"));
        }
    }

    if (actionType != nx::vms::api::ActionType::undefinedAction
        && !nx::vms::event::allActions().contains(actionType))
    {
        return error(lit("Invalid action type"));
    }

    return true;
}

void QnEventLogRequestData::loadFromParams(
    QnResourcePool* resourcePool, const nx::network::rest::Params& params)
{
    filter.loadFromParams(resourcePool, params);
    format = nx::reflect::fromString(params.value(kFormatParam).toStdString(), format);
}

nx::network::rest::Params QnEventLogRequestData::toParams() const
{
    auto result = filter.toParams();
    result.insert(kFormatParam, format);
    return result;
}

bool QnEventLogRequestData::isValid(QString* errorString) const
{
    return filter.isValid(errorString);
}
