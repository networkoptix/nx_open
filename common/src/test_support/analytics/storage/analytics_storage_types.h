#pragma once

#include <analytics/detected_objects_storage/analytics_events_storage_types.h>

namespace nx {
namespace analytics {
namespace storage {
namespace test {

class AttributeDictionary
{
public:
    void initialize(int attributeCount, int valuesPerAttribute);
    common::metadata::Attribute getRandomAttribute() const;
    QString getRandomText() const;

private:
    std::vector<QString> m_words;
};

Filter generateRandomFilter(const AttributeDictionary* attributeDictionary = nullptr);

common::metadata::DetectionMetadataPacketPtr generateRandomPacket(
    int eventCount,
    const AttributeDictionary* attributeDictionary = nullptr);

} // namespace test
} // namespace storage
} // namespace analytics
} // namespace nx
