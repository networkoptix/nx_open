#include "analytics_events_storage_types.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace analytics {
namespace storage {

bool ObjectPosition::operator==(const ObjectPosition& right) const
{
    return deviceId == right.deviceId
        && timestampUsec == right.timestampUsec
        && durationUsec == right.durationUsec
        && boundingBox == right.boundingBox
        && attributes == right.attributes;
}

bool DetectedObject::operator==(const DetectedObject& right) const
{
    return objectId == right.objectId
        && objectTypeId == right.objectTypeId
        && attributes == right.attributes
        && track == right.track;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (ObjectPosition)(DetectedObject),
    (json)(ubjson),
    _analytics_storage_Fields)

//-------------------------------------------------------------------------------------------------

bool Filter::operator==(const Filter& right) const
{
    return objectTypeId == right.objectTypeId
        && objectId == right.objectId
        && timePeriod == right.timePeriod
        && boundingBox == right.boundingBox
        && requiredAttributes == right.requiredAttributes
        && freeText == right.freeText;
}

void serializeToParams(const Filter& filter, QnRequestParamList* params)
{
    if (!filter.deviceId.isNull())
        params->insert(lit("deviceId"), filter.deviceId.toSimpleString());

    for (const auto& objectTypeId: filter.objectTypeId)
        params->insert(lit("objectTypeId"), objectTypeId.toSimpleString());

    if (!filter.objectId.isNull())
        params->insert(lit("objectId"), filter.objectId.toSimpleString());

    // TODO: timePeriod

    if (!filter.boundingBox.isNull())
    {
        params->insert(lit("x1"), QString::number(filter.boundingBox.topLeft().x()));
        params->insert(lit("y1"), QString::number(filter.boundingBox.topLeft().y()));
        params->insert(lit("x2"), QString::number(filter.boundingBox.bottomRight().x()));
        params->insert(lit("y2"), QString::number(filter.boundingBox.bottomRight().y()));
    }

    // TODO: requiredAttributes

    if (!filter.freeText.isEmpty())
    {
        params->insert(
            lit("freeText"),
            QString::fromUtf8(QUrl::toPercentEncoding(filter.freeText)));
    }
}

bool deserializeFromParams(const QnRequestParamList& params, Filter* filter)
{
    if (params.contains(lit("deviceId")))
        filter->deviceId = QnUuid::fromStringSafe(params.value(lit("deviceId")));

    for (const auto& objectTypeId: params.allValues(lit("objectTypeId")))
        filter->objectTypeId.push_back(QnUuid::fromStringSafe(objectTypeId));

    if (params.contains(lit("objectId")))
        filter->objectId = QnUuid::fromStringSafe(params.value(lit("objectId")));

    // TODO: timePeriod.

    if (params.contains(lit("x1")))
    {
        if (!(params.contains(lit("y1")) && params.contains(lit("x2")) && params.contains(lit("y2"))))
            return false;

        filter->boundingBox.setTopLeft(QPointF(
            params.value(lit("x1")).toDouble(),
            params.value(lit("y1")).toDouble()));

        filter->boundingBox.setBottomRight(QPointF(
            params.value(lit("x2")).toDouble(),
            params.value(lit("y2")).toDouble()));
    }

    // TODO: requiredAttributes.

    if (params.contains(lit("freeText")))
    {
        filter->freeText = QUrl::fromPercentEncoding(
            params.value(lit("freeText")).toUtf8());
    }

    return true;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (Filter),
    (json)(ubjson),
    _analytics_storage_Fields)

//-------------------------------------------------------------------------------------------------

ResultCode dbResultToResultCode(nx::utils::db::DBResult dbResult)
{
    switch (dbResult)
    {
        case nx::utils::db::DBResult::ok:
            return ResultCode::ok;

        case nx::utils::db::DBResult::retryLater:
            return ResultCode::retryLater;

        default:
            return ResultCode::error;
    }
}

nx_http::StatusCode::Value toHttpStatusCode(ResultCode resultCode)
{
    switch (resultCode)
    {
        case ResultCode::ok:
            return nx_http::StatusCode::ok;
        case ResultCode::retryLater:
            return nx_http::StatusCode::serviceUnavailable;
        case ResultCode::error:
            return nx_http::StatusCode::internalServerError;
        default:
            return nx_http::StatusCode::internalServerError;
    }
}

ResultCode fromHttpStatusCode(nx_http::StatusCode::Value statusCode)
{
    if (nx_http::StatusCode::isSuccessCode(statusCode))
        return ResultCode::ok;

    switch (statusCode)
    {
        case nx_http::StatusCode::serviceUnavailable:
            return ResultCode::retryLater;
        case nx_http::StatusCode::internalServerError:
            return ResultCode::error;
        default:
            return ResultCode::error;
    }
}

} // namespace storage
} // namespace analytics
} // namespace nx

using namespace nx::analytics::storage;

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::analytics::storage, ResultCode,
    (ResultCode::ok, "ok")
    (ResultCode::retryLater, "retryLater")
    (ResultCode::error, "error")
)
