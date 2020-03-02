#include "analytics_archive.h"

namespace nx::analytics::db {

static const int kRecordSize = QnMetaDataV1::kMotionDataBufferSize + sizeof(BinaryRecordEx);
const std::chrono::seconds AnalyticsArchive::kAggregationInterval{ 5 };

AnalyticsArchive::AnalyticsArchive(const QString& dataDir, const QString& uniqueId):
    base_type(
        "analytics" /*File name prefix*/,
        kRecordSize,
        std::chrono::seconds(kAggregationInterval).count(),
        dataDir,
        uniqueId,
        0 /* Video channel. It is not used yet. */
    )
{
}

bool matchAdditionData(
    int64_t /*timestampMs*/,
    const AnalyticsArchive::Filter& filter,
    const quint8* data, int size)
{
    auto matchIntInList = [](int64_t value, const std::vector<int64_t>& values)
    {
        if (values.empty())
            return true;
        return std::binary_search(values.begin(), values.end(), value);

    };

    BinaryRecordEx* recordEx = (BinaryRecordEx*)data;
    const auto& analyticsFilter = static_cast<const AnalyticsArchive::AnalyticsFilter&>(filter);
    return matchIntInList(recordEx->objectType(), analyticsFilter.objectTypes)
        && matchIntInList(recordEx->attributesHash(), analyticsFilter.allAttributesHash);
}

QnTimePeriodList AnalyticsArchive::matchPeriod(const AnalyticsFilter& filter)
{
    NX_ASSERT_HEAVY_CONDITION(
        std::is_sorted(filter.allAttributesHash.begin(), filter.allAttributesHash.end()));
    NX_ASSERT_HEAVY_CONDITION(
        std::is_sorted(filter.objectTypes.begin(), filter.objectTypes.end()));
    return base_type::matchPeriodInternal(filter, matchAdditionData);
}

AnalyticsArchive::MatchObjectsResult  AnalyticsArchive::matchObjects(
    const AnalyticsFilter& filter)
{
    MatchObjectsResult result;

    NX_ASSERT_HEAVY_CONDITION(
        std::is_sorted(filter.allAttributesHash.begin(), filter.allAttributesHash.end()));
    NX_ASSERT_HEAVY_CONDITION(
        std::is_sorted(filter.objectTypes.begin(), filter.objectTypes.end()));

    auto matchExtraData =
        [&result](int64_t timestampMs,
            const AnalyticsArchive::Filter& filter, const quint8* data, int size)
        {
            bool isMatched = matchAdditionData(timestampMs, filter, data, size);
            if (isMatched)
            {
                BinaryRecordEx* recordEx = (BinaryRecordEx*)data;
                result.data.push_back({recordEx->trackGroupId(), timestampMs});
            }
            return isMatched;
    };

    auto checkLimitInResult =
        [&result, &filter]()
        {
            return result.data.size() >= filter.limit;
        };

    auto timePeriods = base_type::matchPeriodInternal(filter, matchExtraData, checkLimitInResult);
    if (!timePeriods.isEmpty())
    {
        if (filter.sortOrder == Qt::SortOrder::AscendingOrder)
        {
            result.boundingPeriod.startTimeMs = timePeriods.front().startTimeMs;
            result.boundingPeriod.setEndTimeMs(timePeriods.back().endTimeMs());
        }
        else
        {
            result.boundingPeriod.startTimeMs = timePeriods.back().startTimeMs;
            result.boundingPeriod.setEndTimeMs(timePeriods.front().endTimeMs());
        }

    }
    return result;
}

bool AnalyticsArchive::saveToArchive(
    std::chrono::milliseconds startTime,
    const std::vector<QRect>& data,
    uint32_t trackGroupId,
    uint32_t objectType,
    int64_t allAttributesHash)
{
    return saveToArchive<QRect>(
        startTime,
        data,
        trackGroupId,
        objectType,
        allAttributesHash);
}

bool AnalyticsArchive::saveToArchive(
    std::chrono::milliseconds startTime,
    const std::vector<QRectF>& data,
    uint32_t trackGroupId,
    uint32_t objectType,
    int64_t allAttributesHash)
{
    return saveToArchive<QRectF>(
        startTime,
        data,
        trackGroupId,
        objectType,
        allAttributesHash);
}

template <typename RectType>
bool AnalyticsArchive::saveToArchive(
    std::chrono::milliseconds startTime,
    const std::vector<RectType>& data,
    uint32_t trackGroupId,
    uint32_t objectType,
    int64_t allAttributesHash)
{
    QnMetaDataV1Ptr packet(
        new QnMetaDataV1(startTime, 0 /*filler*/, sizeof(BinaryRecordEx) /*extraBufferSize*/));
    packet->timestamp = std::chrono::microseconds(startTime).count();
    packet->m_duration = std::chrono::microseconds(kAggregationInterval).count();

    BinaryRecordEx* recordEx = (BinaryRecordEx*)(packet->data() + QnMetaDataV1::kMotionDataBufferSize);
    recordEx->setTrackGroupId(trackGroupId);
    recordEx->setObjectType(objectType);
    recordEx->setAttributesHash(allAttributesHash);

    for (const auto& rect: data)
        packet->addMotion(rect);

    return saveToArchiveInternal(packet);
}

} // namespace nx::analytics::db
