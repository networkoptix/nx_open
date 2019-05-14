#pragma once

#include <vector>

#include "metadata_archive.h"

namespace nx::vms::server::metadata {

#pragma pack(push, 1)
    struct BinaryRecordEx
    {
        int64_t objectType = 0;
        int64_t attributesHash = 0;
    };
#pragma pack(pop)

class AnalyticsArchive: public MetadataArchive
{
    using base_type = MetadataArchive;
public:
    AnalyticsArchive(const QString& dataDir, const QString& uniqueId);

    bool saveToArchive(
        std::chrono::milliseconds startTime,
        const std::vector<QRectF>& data,
        int64_t objectType,
        int64_t allAttributesHash);

    struct AnalyticsFilter: public Filter
    {
        std::vector<int64_t> objectTypes;
        std::vector<int64_t> allAttributesHash;
    };

    QnTimePeriodList matchPeriod(const AnalyticsFilter& filter);
protected:
    virtual bool matchAdditionData(const Filter& filter, const quint8* data, int size) override;
};

} // namespace nx::vms::server::metadata
