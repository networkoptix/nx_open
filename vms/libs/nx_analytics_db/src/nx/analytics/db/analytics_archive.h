#pragma once

#include <vector>

#include <nx/vms/metadata/metadata_archive.h>

namespace nx::analytics::db {

#pragma pack(push, 1)
    struct BinaryRecordEx
    {
        uint32_t trackGroupId() const { return qFromLittleEndian(m_trackGroupId); }
        uint32_t objectType() const { return qFromLittleEndian(m_objectType); }
        int64_t attributesHash() const { return qFromLittleEndian(m_attributesHash); }

        void setTrackGroupId(uint32_t value) { m_trackGroupId = qToLittleEndian(value); }
        void setObjectType(uint32_t value) { m_objectType = qToLittleEndian(value); }
        void setAttributesHash(int64_t value) { m_attributesHash = qToLittleEndian(value); }
    private:
        uint32_t m_trackGroupId = 0;
        uint32_t m_objectType = 0;
        int64_t m_attributesHash = 0;
    };
#pragma pack(pop)


//-------------------------------------------------------------------------------------------------

class NX_ANALYTICS_DB_API AnalyticsArchive:
    public nx::vms::metadata::MetadataArchive
{
    using base_type = nx::vms::metadata::MetadataArchive;

public:
    static const std::chrono::seconds kAggregationInterval;

    AnalyticsArchive(const QString& dataDir, const QString& uniqueId);

    bool saveToArchive(
        std::chrono::milliseconds startTime,
        const std::vector<QRect>& data,
        uint32_t trackGroupId,
        uint32_t objectType,
        int64_t allAttributesHash);

    bool saveToArchive(
        std::chrono::milliseconds startTime,
        const std::vector<QRectF>& data,
        uint32_t trackGroupId,
        uint32_t objectType,
        int64_t allAttributesHash);

    struct AnalyticsFilter: public Filter
    {
        std::vector<int64_t> objectTypes;
        std::vector<int64_t> allAttributesHash;
    };

    QnTimePeriodList matchPeriod(const AnalyticsFilter& filter);

    struct TrackData
    {
        uint32_t trackGroupId = 0;
        int64_t timestampMs = 0;
    };
    struct MatchObjectsResult
    {
        std::vector<TrackData> data;
        QnTimePeriod boundingPeriod;
    };

    /*
     * Return list of matched trackGroupId, their timestamps and bounding time period.
     * Each trackGroupId from the result list should be resolved to a trackId list via SQL
     * database.
     */
    MatchObjectsResult matchObjects(const AnalyticsFilter& filter);

private:
    template <typename RectType>
    bool saveToArchive(
        std::chrono::milliseconds startTime,
        const std::vector<RectType>& data,
        uint32_t trackGroupId,
        uint32_t objectType,
        int64_t allAttributesHash);
};

} // namespace nx::analytics::db
