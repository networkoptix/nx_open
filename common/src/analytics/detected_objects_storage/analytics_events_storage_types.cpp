#include "analytics_events_storage_types.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace analytics {
namespace storage {

bool DetectionEvent::operator==(const DetectionEvent& right) const
{
    return deviceId == right.deviceId
        && timestampUsec == right.timestampUsec
        && durationUsec == right.durationUsec
        && object == right.object;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (DetectionEvent),
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

void serializeToParams(const Filter& /*filter*/, QnRequestParamList* /*params*/)
{
    // TODO
}

bool deserializeFromParams(const QnRequestParamList& /*params*/, Filter* /*filter*/)
{
    // TODO
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
