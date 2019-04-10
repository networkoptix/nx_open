#include "analytics_events_storage_types.h"

#include <cmath>
#include <sstream>

#include <nx/fusion/model_functions.h>

#include <common/common_globals.h>
#include <utils/math/math.h>

namespace nx {
namespace analytics {
namespace storage {

bool ObjectPosition::operator==(const ObjectPosition& right) const
{
    return deviceId == right.deviceId
        && timestampUsec == right.timestampUsec
        && durationUsec == right.durationUsec
        && equalWithPrecision(boundingBox, right.boundingBox, kCoordinateDecimalDigits)
        && attributes == right.attributes;
}

bool DetectedObject::operator==(const DetectedObject& right) const
{
    return objectAppearanceId == right.objectAppearanceId
        && objectTypeId == right.objectTypeId
        //&& attributes == right.attributes
        && firstAppearanceTimeUsec == right.firstAppearanceTimeUsec
        && lastAppearanceTimeUsec == right.lastAppearanceTimeUsec
        && track == right.track;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (ObjectPosition)(DetectedObject),
    (json)(ubjson),
    _analytics_storage_Fields)

QByteArray serializeTrack(const std::vector<ObjectPosition>& /*track*/)
{
    NX_ASSERT(false);
    // TODO
    return QByteArray();
}

std::vector<ObjectPosition> deserializeTrack(const QByteArray& /*serializedData*/)
{
    // TODO
    return {};
}

//-------------------------------------------------------------------------------------------------

bool Filter::empty() const
{
    return *this == Filter();
}

bool Filter::operator==(const Filter& right) const
{
    return objectTypeId == right.objectTypeId
        && objectAppearanceId == right.objectAppearanceId
        && timePeriod == right.timePeriod
        && equalWithPrecision(boundingBox, right.boundingBox, kCoordinateDecimalDigits)
        && requiredAttributes == right.requiredAttributes
        && freeText == right.freeText
        && sortOrder == right.sortOrder;
}

bool Filter::operator!=(const Filter& right) const
{
    return !(*this == right);
}

void serializeToParams(const Filter& filter, QnRequestParamList* params)
{
    for (const auto deviceId: filter.deviceIds)
        params->insert(lit("deviceId"), deviceId.toSimpleString());

    for (const auto& objectTypeId: filter.objectTypeId)
        params->insert(lit("objectTypeId"), objectTypeId);

    if (!filter.objectAppearanceId.isNull())
        params->insert(lit("objectAppearanceId"), filter.objectAppearanceId.toSimpleString());

    params->insert(lit("startTime"), QnLexical::serialized(filter.timePeriod.startTimeMs));
    params->insert(lit("endTime"), QnLexical::serialized(filter.timePeriod.endTimeMs()));

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

    if (filter.maxObjectsToSelect > 0)
        params->insert(lit("limit"), QString::number(filter.maxObjectsToSelect));

    if (filter.maxTrackSize > 0)
        params->insert(lit("maxTrackSize"), QString::number(filter.maxTrackSize));

    params->insert(lit("sortOrder"), QnLexical::serialized(filter.sortOrder));
}

bool deserializeFromParams(const QnRequestParamList& params, Filter* filter)
{
    for (const auto& deviceIdStr: params.allValues(lit("deviceId")))
        filter->deviceIds.push_back(QnUuid::fromStringSafe(deviceIdStr));

    for (const auto& objectTypeId: params.allValues(lit("objectTypeId")))
        filter->objectTypeId.push_back(objectTypeId);

    if (params.contains(lit("objectAppearanceId")))
        filter->objectAppearanceId = QnUuid::fromStringSafe(params.value(lit("objectAppearanceId")));

    filter->timePeriod.setStartTime(
        std::chrono::milliseconds(params.value("startTime").toLongLong()));
    filter->timePeriod.setDuration(
        std::chrono::milliseconds(QnTimePeriod::kInfiniteDuration));
    if (params.contains("endTime"))
    {
        filter->timePeriod.setEndTime(
            std::chrono::milliseconds(params.value("endTime").toLongLong()));
    }

    QnLexical::deserialize(params.value(lit("sortOrder")), &filter->sortOrder);

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

    if (params.contains(lit("limit")))
        filter->maxObjectsToSelect = params.value(lit("limit")).toInt();

    if (params.contains(lit("maxObjectsToSelect")))
        filter->maxObjectsToSelect = params.value(lit("maxObjectsToSelect")).toInt();

    if (params.contains(lit("maxTrackSize")))
        filter->maxTrackSize = params.value(lit("maxTrackSize")).toInt();

    return true;
}

::std::ostream& operator<<(::std::ostream& os, const Filter& filter)
{
    for (const auto& deviceId: filter.deviceIds)
        os << "deviceId " << deviceId.toSimpleString().toStdString() << "; ";
    if (!filter.objectTypeId.empty())
        os << "objectTypeId " << lm("%1").container(filter.objectTypeId).toStdString() << "; ";
    if (!filter.objectAppearanceId.isNull())
        os << "objectAppearanceId " << filter.objectAppearanceId.toSimpleString().toStdString() << "; ";
    os << "timePeriod [" << filter.timePeriod.startTimeMs << ", " <<
        filter.timePeriod.durationMs << "]; ";
    if (!filter.boundingBox.isNull())
    {
        os << "boundingBox [" <<
            filter.boundingBox.topLeft().x() << ", " <<
            filter.boundingBox.topLeft().y() << ", " <<
            filter.boundingBox.bottomRight().x() << ", " <<
            filter.boundingBox.bottomRight().y() << "]; ";
    }
    if (!filter.freeText.isEmpty())
        os << "freeText \"" << filter.freeText.toStdString() << "\"; ";

    os << "maxObjectsToSelect " << filter.maxObjectsToSelect << "; ";
    os << "maxTrackSize " << filter.maxTrackSize << "; ";
    os << "sortOrder " <<
        (filter.sortOrder == Qt::SortOrder::DescendingOrder ? "DESC" : "ASC");

    return os;
}

QString toString(const Filter& filter)
{
    std::ostringstream stringStream;
    stringStream << filter;
    return QString::fromStdString(stringStream.str());
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (Filter),
    (json)(ubjson),
    _analytics_storage_Fields)

//-------------------------------------------------------------------------------------------------

ResultCode dbResultToResultCode(nx::sql::DBResult dbResult)
{
    switch (dbResult)
    {
        case nx::sql::DBResult::ok:
            return ResultCode::ok;

        case nx::sql::DBResult::retryLater:
            return ResultCode::retryLater;

        default:
            return ResultCode::error;
    }
}

nx::network::http::StatusCode::Value toHttpStatusCode(ResultCode resultCode)
{
    switch (resultCode)
    {
        case ResultCode::ok:
            return nx::network::http::StatusCode::ok;
        case ResultCode::retryLater:
            return nx::network::http::StatusCode::serviceUnavailable;
        case ResultCode::error:
            return nx::network::http::StatusCode::internalServerError;
        default:
            return nx::network::http::StatusCode::internalServerError;
    }
}

ResultCode fromHttpStatusCode(nx::network::http::StatusCode::Value statusCode)
{
    if (nx::network::http::StatusCode::isSuccessCode(statusCode))
        return ResultCode::ok;

    switch (statusCode)
    {
        case nx::network::http::StatusCode::serviceUnavailable:
            return ResultCode::retryLater;
        case nx::network::http::StatusCode::internalServerError:
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
