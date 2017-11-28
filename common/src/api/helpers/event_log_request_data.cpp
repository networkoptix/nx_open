#include "event_log_request_data.h"

#include <api/helpers/camera_id_helper.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/event/events/abstract_event.h>

#include <nx/fusion/model_functions.h>

#include <nx/utils/string.h>

namespace {

static const QString kCameraIdParam = lit("cameraId");
static const QString kDeprecatedPhysicalIdParam = lit("physicalId");
static const QString kStartPeriodParam = lit("from");
static const QString kEndPeriodParam = lit("to");
static const QString kEventTypeParam = lit("event_type");
static const QString kEventSubtypeParam = lit("event_subtype");
static const QString kActionTypeParam = lit("action_type");
static const QString kRuleIdParam = lit("brule_id");
static const QString kFormatParam(lit("format"));

static constexpr int kInvalidStartTime = -1;

} // namespace

void QnEventLogFilterData::loadFromParams(QnResourcePool* resourcePool,
    const QnRequestParamList& params)
{
    nx::camera_id_helper::findAllCamerasByFlexibleIds(
        resourcePool,
        &cameras,
        params,
        {kCameraIdParam, kDeprecatedPhysicalIdParam});

    const auto startTimeUs = params.value(kStartPeriodParam);
    period.startTimeMs = startTimeUs.isEmpty()
        ? kInvalidStartTime
        : nx::utils::parseDateTimeMsec(startTimeUs);

    const auto endTimeUs = params.value(kEndPeriodParam);
    period.durationMs = endTimeUs.isEmpty()
        ? QnTimePeriod::infiniteDuration()
        : nx::utils::parseDateTimeMsec(endTimeUs) - period.startTimeMs;

    eventType = QnLexical::deserialized<nx::vms::event::EventType>(
        params.value(kEventTypeParam),
        eventType);

    eventSubtype = QnLexical::deserialized<QnUuid>(params.value(kEventSubtypeParam));

    actionType = QnLexical::deserialized<nx::vms::event::ActionType>(
        params.value(kActionTypeParam),
        actionType);

    ruleId = QnLexical::deserialized<QnUuid>(params.value(kRuleIdParam));
}

QnRequestParamList QnEventLogFilterData::toParams() const
{
    QnRequestParamList result;

    for (const auto& camera: cameras)
        result.insert(kCameraIdParam, QnLexical::serialized(camera->getId()));

    result.insert(kStartPeriodParam, QnLexical::serialized(period.startTimeMs));

    if (!period.isInfinite())
        result.insert(kEndPeriodParam, QnLexical::serialized(period.endTimeMs()));

    if (eventType != nx::vms::event::undefinedEvent)
        result.insert(kEventTypeParam, QnLexical::serialized(eventType));

    if (!eventSubtype.isNull())
        result.insert(kEventSubtypeParam, QnLexical::serialized(eventSubtype));

    if (actionType != nx::vms::event::undefinedAction)
        result.insert(kActionTypeParam, QnLexical::serialized(actionType));

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

    if (eventType != nx::vms::event::undefinedEvent
        && !nx::vms::event::allEvents().contains(eventType)
        && !nx::vms::event::hasChild(eventType))
    {
        return error(lit("Invalid event type"));
    }

    if (actionType != nx::vms::event::undefinedAction
        && !nx::vms::event::allActions().contains(actionType))
    {
        return error(lit("Invalid action type"));
    }

    return true;
}

void QnEventLogRequestData::loadFromParams(QnResourcePool* resourcePool,
    const QnRequestParamList& params)
{
    filter.loadFromParams(resourcePool, params);
    format = QnLexical::deserialized(params.value(kFormatParam), format);
}

QnRequestParamList QnEventLogRequestData::toParams() const
{
    QnRequestParamList result = filter.toParams();
    result.insert(kFormatParam, QnLexical::serialized(format));
    return result;
}

bool QnEventLogRequestData::isValid(QString* errorString) const
{
    return filter.isValid(errorString);
}
