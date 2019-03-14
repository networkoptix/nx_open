#include "connection_statistics_info.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace hpm {
namespace stats {

ConnectSession::ConnectSession():
    resultCode(api::NatTraversalResultCode::ok)
{
}

bool ConnectSession::operator==(const ConnectSession& rhs) const
{
    return
        startTime == rhs.startTime &&
        endTime == rhs.endTime &&
        resultCode == rhs.resultCode &&
        sessionId == rhs.sessionId &&
        originatingHostEndpoint == rhs.originatingHostEndpoint &&
        originatingHostName == rhs.originatingHostName &&
        destinationHostEndpoint == rhs.destinationHostEndpoint &&
        destinationHostName == rhs.destinationHostName;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (ConnectSession),
    (sql_record),
    _Fields)

} // namespace stats
} // namespace hpm
} // namespace nx
