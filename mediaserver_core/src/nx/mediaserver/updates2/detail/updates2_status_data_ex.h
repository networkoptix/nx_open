#pragma once

#include <nx/api/updates2/updates2_status_data.h>

namespace nx {
namespace mediaserver {
namespace updates2 {
namespace detail {

struct Updates2StatusDataEx: api::Updates2StatusData
{
    qint64 lastRefreshTime = 0;

    const api::Updates2StatusData& base() const;
    Updates2StatusDataEx(
        qint64 lastRefreshTime,
        const QnUuid& serverId,
        StatusCode status,
        QString message = QString(),
        double progress = 0.0);
    Updates2StatusDataEx(const api::Updates2StatusData& other);
    Updates2StatusDataEx() = default;
};

bool operator == (const Updates2StatusDataEx& lhs, const Updates2StatusDataEx& rhs);
bool operator != (const Updates2StatusDataEx& lhs, const Updates2StatusDataEx& rhs);


#define Updates2StatusDataEx_Fields Updates2StatusData_Fields (lastRefreshTime)
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((Updates2StatusDataEx), (json))

} // namespace detail
} // namespace updates2
} // namespace mediaserver
} // namespace nx
