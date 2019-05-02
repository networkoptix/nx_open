#pragma once

#include <chrono>
#include <set>
#include <vector>

#include <QtCore/QRectF>
#include <QtCore/QString>

#include <nx/utils/uuid.h>

#include <recording/time_period_list.h>

namespace nx::analytics::storage {

class AnalyticsArchive
{
public:
    struct Filter
    {
        std::vector<QRectF> region;
        std::chrono::milliseconds startTime;
        std::chrono::milliseconds endTime;
        std::chrono::milliseconds detailLevel = std::chrono::milliseconds::zero();
        Qt::SortOrder sortOrder = Qt::AscendingOrder;
        int limit = -1;
        std::set<int> objectTypes;
        std::set<int> allAttributesHash;
    };

    AnalyticsArchive(const QString& dataDir, const QnUuid& resourceId);
    virtual ~AnalyticsArchive();

    bool saveToArchive(
        std::chrono::milliseconds timestamp,
        const std::vector<QRectF>& region,
        int objectType,
        long long allAttributesHash);

    QnTimePeriodList matchPeriod(const Filter& filter);

private:
    const QString m_dataDir;
    const QnUuid m_resourceId;
};

} // namespace nx::analytics::storage
