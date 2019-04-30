#pragma once

#include <set>
#include <chrono>

#include "metadata_archive.h"
#include "qnamespace.h"

namespace nx::vms::server::metadata {


class AnalyticsArchive: public MetadataArchive
{
public:
    AnalyticsArchive(const QString& dataDir, const QString& uniqueId);

    bool saveToArchive(const QList<QRectF>& data, int objectType, int allAttributesHash);

    struct AnalyticsFilter: public Filter
    {
        std::set<int> objectTypes;
        std::set<int> allAttributesHash;
    };

    QnTimePeriodList matchPeriod(const AnalyticsFilter& filter);
protected:
};

} // namespace nx::vms::server::metadata
