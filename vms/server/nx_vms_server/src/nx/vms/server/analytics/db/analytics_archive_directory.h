#pragma once

//#define USE_IN_MEMORY_ARCHIVE

#include <vector>

#include <nx/vms/server/metadata/analytics_archive.h>

#include "analytics_archive.h"
#include "media_server/media_server_module.h"

namespace nx::analytics::db {

#ifdef USE_IN_MEMORY_ARCHIVE
using AnalyticsArchiveImpl = AnalyticsArchive;
#else
using AnalyticsArchiveImpl = nx::vms::server::metadata::AnalyticsArchive;
#endif

class AnalyticsArchiveDirectory
{
public:
    using Filter = AnalyticsArchive::Filter;

    AnalyticsArchiveDirectory(
        QnMediaServerModule* mediaServerModule,
        const QString& dataDir);
    
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
    QnMediaServerModule* m_mediaServerModule = nullptr;
    const QString m_dataDir;
    std::map<QnUuid, std::unique_ptr<AnalyticsArchiveImpl>> m_deviceIdToArchive;
};

} // namespace nx::analytics::db
