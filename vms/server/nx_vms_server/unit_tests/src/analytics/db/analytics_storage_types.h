#pragma once

#include <analytics/db/analytics_db_types.h>

namespace nx::analytics::db::test {

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

QRectF generateRandomRectf();

} // namespace nx::analytics::db::test
