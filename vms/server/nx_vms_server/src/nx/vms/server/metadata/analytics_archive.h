#pragma once

#include "metadata_archive.h"

namespace nx::vms::server::metadata {

#pragma pack(push, 1)
    struct BinaryRecordEx
    {
        int objectType = 0;
        int attributesHash = 0;
    };
#pragma pack(pop)

class AnalyticsArchive: public MetadataArchive
{
    using base_type = MetadataArchive;
public:
    AnalyticsArchive(const QString& dataDir, const QString& uniqueId);

    bool saveToArchive(
        std::chrono::milliseconds startTime,
        const QList<QRectF>& data, 
        int objectType, int allAttributesHash);

    struct AnalyticsFilter: public Filter
    {
        std::vector<int> objectTypes;
        std::vector<int> allAttributesHash;
    };

    QnTimePeriodList matchPeriod(const AnalyticsFilter& filter);
protected:
    virtual bool matchAdditionData(const Filter& filter, const quint8* data, int size) override;
};

} // namespace nx::vms::server::metadata
