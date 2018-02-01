#include "updates2_status_data_ex.h"
#include <nx/fusion/model_functions.h>
#include <utils/common/synctime.h>

namespace nx {
namespace mediaserver {
namespace updates2 {
namespace detail {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((Updates2StatusDataEx), (json), _Fields)

const api::Updates2StatusData& Updates2StatusDataEx::base() const
{
    return *this;
}

Updates2StatusDataEx::Updates2StatusDataEx(
    qint64 lastRefreshTime,
    const QnUuid& serverId,
    StatusCode status,
    QString message,
    double progress)
    :
    api::Updates2StatusData(serverId, status, std::move(message), progress),
    lastRefreshTime(lastRefreshTime)
{}

//Updates2StatusDataEx::Updates2StatusDataEx(const api::Updates2StatusData& other):
//    Updates2StatusDataEx(
//        qnSyncTime->currentMSecsSinceEpoch(),
//        other.serverId, other.state,
//        other.message, other.progress)
//{}

//Updates2StatusDataEx& Updates2StatusDataEx::operator=(const api::Updates2StatusData& other)
//{

//}

void Updates2StatusDataEx::fromBase(const api::Updates2StatusData& other)
{
    lastRefreshTime = qnSyncTime->currentMSecsSinceEpoch();
    static_cast<Updates2StatusData&>(*this) = other;
}

void Updates2StatusDataEx::clone(const Updates2StatusDataEx& other)
{
    lastRefreshTime = other.lastRefreshTime;
    files = other.files;
    static_cast<Updates2StatusData&>(*this) = other;
}

bool operator == (const Updates2StatusDataEx& lhs, const Updates2StatusDataEx& rhs)
{
    return lhs.lastRefreshTime == rhs.lastRefreshTime
        && lhs.serverId == rhs.serverId
        && lhs.message == rhs.message
        && std::abs(lhs.progress - rhs.progress) < std::numeric_limits<double>::epsilon()
        && lhs.state == rhs.state
        && lhs.files == rhs.files;
}

bool operator != (const Updates2StatusDataEx& lhs, const Updates2StatusDataEx& rhs)
{
    return !(lhs == rhs);
}

} // namespace detail
} // namespace updates2
} // namespace mediaserver
} // namespace nx
