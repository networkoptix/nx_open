#include "updates2_status_data_ex.h"
#include <nx/fusion/model_functions.h>

namespace nx {
namespace update {
namespace manager {
namespace detail {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((Updates2StatusDataEx), (json)(eq), _Fields)

const api::Updates2StatusData& Updates2StatusDataEx::base() const
{
    return *this;
}

Updates2StatusDataEx::Updates2StatusDataEx(
    qint64 lastRefreshTime,
    const QnUuid& serverId,
    StatusCode status,
    QString message,
    const QList<api::TargetVersionWithEula>& targetVersions,
    const QString& releaseNotesUrl,
    double progress)
    :
    api::Updates2StatusData(serverId, status, std::move(message), targetVersions, releaseNotesUrl,
        progress),
    lastRefreshTime(lastRefreshTime)
{}

void Updates2StatusDataEx::fromBase(const api::Updates2StatusData& other)
{
    lastRefreshTime = QDateTime::currentMSecsSinceEpoch();
    static_cast<Updates2StatusData&>(*this) = other;
}

void Updates2StatusDataEx::clone(const Updates2StatusDataEx& other)
{
    lastRefreshTime = other.lastRefreshTime;
    file = other.file;
    static_cast<Updates2StatusData&>(*this) = other;
}

} // namespace detail
} // namespace manager
} // namespace update
} // namespace nx
