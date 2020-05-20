#pragma once

#include <analytics/db/analytics_db_types.h>

namespace nx::analytics::db::test {

class AttributeDictionary
{
public:
    void initialize(int attributeCount, int valuesPerAttribute);
    void initialize(std::vector<common::metadata::Attribute> attributes);

    common::metadata::Attribute getRandomAttribute() const;
    QString getRandomText() const;

private:
    std::vector<common::metadata::Attribute> m_attributes;
};

Filter generateRandomFilter(const AttributeDictionary* attributeDictionary = nullptr);

common::metadata::ObjectMetadataPacketPtr generateRandomPacket(
    int eventCount,
    const AttributeDictionary* attributeDictionary = nullptr);

QRectF generateRandomRectf();

} // namespace nx::analytics::db::test
