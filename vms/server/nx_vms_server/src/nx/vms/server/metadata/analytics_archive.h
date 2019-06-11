#pragma once

#include <vector>

#include "metadata_archive.h"

namespace nx::vms::server::metadata {

#pragma pack(push, 1)
    struct BinaryRecordEx
    {
        uint32_t objectsGroupId() const { return qFromLittleEndian(m_objectsGroupId); }
        uint32_t objectType() const { return qFromLittleEndian(m_objectType); }
        int64_t attributesHash() const { return qFromLittleEndian(m_attributesHash); }

        void setObjectsGroupId(uint32_t value) { m_objectsGroupId = qToLittleEndian(value); }
        void setObjectType(uint32_t value) { m_objectType = qToLittleEndian(value); }
        void setAttributesHash(int64_t value) { m_attributesHash = qToLittleEndian(value); }
    private:
        uint32_t m_objectsGroupId = 0;
        uint32_t m_objectType = 0;
        int64_t m_attributesHash = 0;
    };
#pragma pack(pop)

class AnalyticsArchive: public MetadataArchive
{
    using base_type = MetadataArchive;
public:

    static const std::chrono::seconds kAggregationInterval;

    AnalyticsArchive(const QString& dataDir, const QString& uniqueId);

    template <typename RectType>
    bool saveToArchive(
        std::chrono::milliseconds startTime,
        const std::vector<RectType>& data,
        uint32_t objectsGroupId,
        uint32_t objectType,
        int64_t allAttributesHash);

    struct AnalyticsFilter: public Filter
    {
        std::vector<int64_t> objectTypes;
        std::vector<int64_t> allAttributesHash;
    };

    QnTimePeriodList matchPeriod(const AnalyticsFilter& filter);

    struct ObjectData
    {
        uint32_t objectGroupId = 0;
        int64_t timestampMs = 0;
    };
    struct MatchObjectsResult
    {
        std::vector<ObjectData> data;
        QnTimePeriod boundingPeriod;
    };

    /*
     * Return list of matched objectGroupId, their timestamps and bounding time period.
     * Each objectGroupId from the result list should be resolved to a objectId list via SQL database.
     */
    MatchObjectsResult matchObjects(const AnalyticsFilter& filter);
};

} // namespace nx::vms::server::metadata
