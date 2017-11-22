#include "analytics_events_storage_types.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace mediaserver {
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

void serializeToParams(const Filter& /*filter*/, QnRequestParamList* /*params*/)
{
    // TODO
}

bool deserializeFromParams(const QnRequestParamList& /*params*/, Filter* /*filter*/)
{
    // TODO
    return false;
}

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

} // namespace storage
} // namespace analytics
} // namespace mediaserver
} // namespace nx

using namespace nx::mediaserver::analytics::storage;

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::mediaserver::analytics::storage, ResultCode,
    (ResultCode::ok, "ok")
    (ResultCode::retryLater, "retryLater")
    (ResultCode::error, "error")
)
