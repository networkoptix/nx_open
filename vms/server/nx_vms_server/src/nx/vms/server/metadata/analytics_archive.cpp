#include "analytics_archive.h"

namespace nx::vms::server::metadata {

static const int kRecordSize = QnMetaDataV1::kMotionDataBufferSize + sizeof(BinaryRecordEx);
const std::chrono::seconds AnalyticsArchive::kAggregationInterval{ 5 };

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

bool matchAdditionData(const AnalyticsArchive::Filter& filter, const quint8* data, int size)
{
    auto matchIntInList = [](int64_t value, const std::vector<int64_t>& values)
    {
        if (values.empty())
            return true;
        return std::any_of(values.begin(), values.end(),
            [&](const int64_t& data) { return data == value; });

    };

    BinaryRecordEx* recordEx = (BinaryRecordEx*)data;
    const auto& analyticsFilter = static_cast<const AnalyticsArchive::AnalyticsFilter&>(filter);
    return matchIntInList(recordEx->objectType, analyticsFilter.objectTypes)
        && matchIntInList(recordEx->attributesHash, analyticsFilter.allAttributesHash);
}

QnTimePeriodList AnalyticsArchive::matchPeriod(const AnalyticsFilter& filter)
{
    return base_type::matchPeriodInternal(filter, matchAdditionData);
}

std::tuple<std::vector<uint32_t>, QnTimePeriod>  AnalyticsArchive::matchObjects(
    const AnalyticsFilter& filter)
{
    std::vector<uint32_t> result;

    auto matchExtraData =
        [&result](const AnalyticsArchive::Filter& filter, const quint8* data, int size)
        {
            bool isMatched = matchAdditionData(filter, data, size);
            if (isMatched)
            {
                BinaryRecordEx* recordEx = (BinaryRecordEx*)data;
                result.push_back(recordEx->objectsGroupId);
            }
            return isMatched;
    };

    auto checkLimitInResult =
        [&result, &filter]()
        {
            return result.size() >= filter.limit;
        };

    auto timePeriods = base_type::matchPeriodInternal(filter, matchExtraData, checkLimitInResult);
    QnTimePeriod outPeriod;
    if (!timePeriods.isEmpty())
    {
        if (filter.sortOrder == Qt::SortOrder::AscendingOrder)
        {
            outPeriod.startTimeMs = timePeriods.front().startTimeMs;
            outPeriod.setEndTimeMs(timePeriods.back().endTimeMs());
        }
        else
        {
            outPeriod.startTimeMs = timePeriods.back().startTimeMs;
            outPeriod.setEndTimeMs(timePeriods.front().endTimeMs());
        }

    }
    return { result, outPeriod };
}

template <typename RectType>
bool AnalyticsArchive::saveToArchive(
    std::chrono::milliseconds startTime,
    const std::vector<RectType>& data,
    uint32_t objectsGroupId,
    uint32_t objectType,
    int64_t allAttributesHash)
{
    QnMetaDataV1Ptr packet(
        new QnMetaDataV1(0 /*filler*/, sizeof(BinaryRecordEx) /*extraBufferSize*/));
    packet->timestamp = std::chrono::microseconds(startTime).count();
    packet->m_duration = std::chrono::microseconds(kAggregationInterval).count();

    BinaryRecordEx* recordEx = (BinaryRecordEx*)(packet->data() + QnMetaDataV1::kMotionDataBufferSize);
    recordEx->objectsGroupId = objectsGroupId;
    recordEx->objectType = objectType;
    recordEx->attributesHash = allAttributesHash;

    for (const auto& rect: data)
        packet->addMotion(rect);

    return saveToArchiveInternal(packet);
}

template bool AnalyticsArchive::saveToArchive<QRect>(
    std::chrono::milliseconds startTime,
    const std::vector<QRect>& data,
    uint32_t objectsGroupId,
    uint32_t objectType,
    int64_t allAttributesHash);

template bool AnalyticsArchive::saveToArchive<QRectF>(
    std::chrono::milliseconds startTime,
    const std::vector<QRectF>& data,
    uint32_t objectsGroupId,
    uint32_t objectType,
    int64_t allAttributesHash);

} // namespace nx::vms::server::metadata
