#pragma once

#include <chrono>
#include <limits>
#include <set>
#include <vector>

#include <QtCore/QRectF>
#include <QtCore/QString>
#include <QtGui/QRegion>

#include <nx/utils/uuid.h>

#include <nx/vms/server/metadata/analytics_archive.h>
#include <recording/time_period_list.h>

namespace nx::analytics::db {

class AnalyticsArchive
{
public:
    using Filter = nx::vms::server::metadata::AnalyticsArchive::AnalyticsFilter;

    AnalyticsArchive(const QString& dataDir, const QnUuid& resourceId);
    virtual ~AnalyticsArchive();

    /**
     * @param region Region on the search grid. (Qn::kMotionGridWidth * Qn::kMotionGridHeight).
     */
    bool saveToArchive(
        std::chrono::milliseconds timestamp,
        const std::vector<QRect>& region,
        int64_t objectType,
        int64_t allAttributesHash);

    QnTimePeriodList matchPeriod(const Filter& filter);

private:
    struct Item
    {
        std::chrono::milliseconds timestamp = std::chrono::milliseconds::zero();
        QRect rect;
        int64_t objectType = -1;
        int64_t allAttributesId = -1;
    };

    const QString m_dataDir;
    const QnUuid m_resourceId;
    std::vector<Item> m_items;

    bool satisfiesFilter(
        const Item& item,
        const Filter& filter,
        const std::vector<QRect>& regionFilter);
};

} // namespace nx::analytics::db
