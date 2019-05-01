#include "analytics_archive.h"

namespace nx::vms::server::metadata {

static const std::chrono::seconds kAggregationInterval{5};
static const int kRecordSize = QnMetaDataV1::kMotionDataBufferSize + sizeof(BinaryRecordEx);

AnalyticsArchive::AnalyticsArchive(const QString& dataDir, const QString& uniqueId):
    MetadataArchive(
        "analytics" /*File name prefix*/,
        kRecordSize,
        std::chrono::seconds(kAggregationInterval).count(),
        dataDir,
        uniqueId,
        0 /* Video channel. It is not used yet. */
    )
{
}

QnTimePeriodList AnalyticsArchive::matchPeriod(const AnalyticsFilter& filter)
{
    return base_type::matchPeriodInternal(filter);
}

bool AnalyticsArchive::matchAdditionData(const Filter& filter, const quint8* data, int size) 
{ 
    auto matchIntInList = [](int value, const std::vector<int>& values)
    {
        if (values.empty())
            return true;
        return std::any_of(values.begin(), values.end(),
            [&](const int& data) { return data == value; });

    };

    BinaryRecordEx* recordEx = (BinaryRecordEx*)data;
    const auto& analyticsFilter = static_cast<const AnalyticsFilter&>(filter);
    return matchIntInList(recordEx->objectType, analyticsFilter.objectTypes)
        && matchIntInList(recordEx->attributesHash, analyticsFilter.allAttributesHash);
}

bool AnalyticsArchive::saveToArchive(
    std::chrono::milliseconds startTime,
    const QList<QRectF>& data, 
    int objectType, 
    int allAttributesHash)
{
    QnMetaDataV1Ptr packet(
        new QnMetaDataV1(0 /*filler*/, sizeof(BinaryRecordEx) /*extraBufferSize*/));
    packet->timestamp = std::chrono::microseconds(startTime).count();
    packet->m_duration = std::chrono::microseconds(kAggregationInterval).count();

    BinaryRecordEx* recordEx = (BinaryRecordEx*)(packet->data() + QnMetaDataV1::kMotionDataBufferSize);
    recordEx->objectType = objectType;
    recordEx->attributesHash = allAttributesHash;

    for (const auto& rectF: data)
        packet->addMotion(rectF);

    return saveToArchiveInternal(packet);
}

} // namespace nx::vms::server::metadata
