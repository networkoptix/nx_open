#pragma once

#include <vector>

#include "analytics_archive.h"

namespace nx::analytics::storage {

class AnalyticsArchiveDirectory
{
public:
    using Filter = AnalyticsArchive::Filter;

    AnalyticsArchiveDirectory(const QString& dataDir);
    virtual ~AnalyticsArchiveDirectory() = default;

    bool saveToArchive(
        const QnUuid& deviceId,
        std::chrono::milliseconds timestamp,
        const std::vector<QRectF>& region,
        int objectType,
        long long allAttributesHash);

    QnTimePeriodList matchPeriods(
        const std::vector<QnUuid>& deviceIds,
        const Filter& filter);

    QnTimePeriodList matchPeriods(
        const QnUuid& deviceId,
        const Filter& filter);

private:
    const QString m_dataDir;
    std::map<QnUuid, std::unique_ptr<AnalyticsArchive>> m_deviceIdToArchive;
};

} // namespace nx::analytics::storage
