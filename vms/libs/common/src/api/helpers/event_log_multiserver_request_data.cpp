#include "event_log_multiserver_request_data.h"

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
static const QString kSortOrderParam = lit("sortOrder");
static const QString kLimitParam = lit("limit");

static constexpr int kInvalidStartTime = -1;

} // namespace

QnEventLogMultiserverRequestData::QnEventLogMultiserverRequestData()
{
    format = Qn::UbjsonFormat;
}

QnEventLogMultiserverRequestData::QnEventLogMultiserverRequestData(
    QnResourcePool* resourcePool,
    const QnRequestParamList& params)
    :
    QnEventLogMultiserverRequestData()
{
    loadFromParams(resourcePool, params);
}

void QnEventLogMultiserverRequestData::loadFromParams(
    QnResourcePool* resourcePool,
    const QnRequestParamList& params)
{
    QnMultiserverRequestData::loadFromParams(resourcePool, params);
    filter.loadFromParams(resourcePool, params);

    order = QnLexical::deserialized<Qt::SortOrder>(params.value(kSortOrderParam));

    limit = params.contains(kLimitParam)
        ? qMax(0, params.value(kLimitParam).toInt())
        : kNoLimit;
}

void QnEventLogMultiserverRequestData::loadFromParams(
    QnResourcePool* resourcePool,
    const QnRequestParams& params)
{
    QnRequestParamList paramList;
    for (auto iter = params.cbegin(); iter != params.cend(); ++iter)
        paramList.insert(iter.key(), iter.value());

    loadFromParams(resourcePool, paramList);
}

QnRequestParamList QnEventLogMultiserverRequestData::toParams() const
{
    QnRequestParamList result = QnMultiserverRequestData::toParams();
    result.insert(filter.toParams());

    if (order != kDefaultOrder)
        result.insert(kSortOrderParam, QnLexical::serialized(order));

    if (limit != kNoLimit)
        result.insert(kLimitParam, QnLexical::serialized(limit));

    return result;
}

bool QnEventLogMultiserverRequestData::isValid() const
{
    return QnMultiserverRequestData::isValid() && filter.isValid(nullptr);
}
