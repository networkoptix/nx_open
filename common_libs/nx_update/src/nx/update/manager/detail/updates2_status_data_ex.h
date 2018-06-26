#pragma once

#include <nx/api/updates2/updates2_status_data.h>

namespace nx {
namespace update {
namespace manager {
namespace detail {

struct NX_UPDATE_API Updates2StatusDataEx: api::Updates2StatusData
{
    qint64 lastRefreshTime = 0;
    QString file;

    const api::Updates2StatusData& base() const;
    Updates2StatusDataEx(
        qint64 lastRefreshTime,
        const QnUuid& serverId,
        StatusCode status,
        QString message = QString(),
        const QList<api::TargetVersionWithEula>& targetVersions = QList<api::TargetVersionWithEula>(),
        double progress = 0.0);
    Updates2StatusDataEx(const api::Updates2StatusData& other) = delete;
    Updates2StatusDataEx& operator=(const api::Updates2StatusData& other) = delete;
    Updates2StatusDataEx& operator=(const Updates2StatusDataEx& other) = delete;
    Updates2StatusDataEx& operator=(Updates2StatusDataEx&& other) = delete;
    Updates2StatusDataEx() = default;

    void fromBase(const api::Updates2StatusData& other);
    void clone(const Updates2StatusDataEx& other);
};

#define Updates2StatusDataEx_Fields Updates2StatusData_Fields (lastRefreshTime)(file)
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((Updates2StatusDataEx), (json)(eq))

} // namespace detail
} // namespace manager
} // namespace update
} // namespace nx
