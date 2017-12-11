#include "analytics_storage_types.h"

#include <algorithm>

#include <QtCore/QStringList>

#include <nx/utils/random.h>

namespace nx {
namespace analytics {
namespace storage {
namespace test {

namespace {

QString generateFreeText()
{
    const int wordCount = nx::utils::random::number<int>(0, 3);
    QStringList words;
    for (int i = 0; i < wordCount; ++i)
        words.push_back(QString::fromLatin1(nx::utils::random::generateName(7)));
    return words.join(L' ');
}

} // namespace

//-------------------------------------------------------------------------------------------------

void AttributeDictionary::initialize(int attributeCount, int valuesPerAttribute)
{
    m_words.resize(attributeCount * valuesPerAttribute);
    for (auto& word: m_words)
        word = QString::fromLatin1(nx::utils::random::generateName(7));
}

common::metadata::Attribute AttributeDictionary::getRandomAttribute() const
{
    return common::metadata::Attribute{
        nx::utils::random::choice(m_words),
        nx::utils::random::choice(m_words)};
}

QString AttributeDictionary::getRandomText() const
{
    const int elementsToConcat = nx::utils::random::number<int>(1, 2);
    QStringList result;
    for (int i = 0; i < elementsToConcat; ++i)
        result.push_back(nx::utils::random::choice(m_words));
    return result.join(L' ');
}

//-------------------------------------------------------------------------------------------------

Filter generateRandomFilter(const AttributeDictionary* attributeDictionary)
{
    Filter filter;

    if (nx::utils::random::number<bool>())
        filter.deviceId = QnUuid::createUuid();

    filter.objectTypeId.resize(nx::utils::random::number<int>(0, 5));
    std::generate(
        filter.objectTypeId.begin(), filter.objectTypeId.end(),
        []() { return QnUuid::createUuid(); });

    if (nx::utils::random::number<bool>())
        filter.objectId = QnUuid::createUuid();

    // TODO: timePeriod.

    if (nx::utils::random::number<bool>())
    {
        filter.boundingBox.setCoords(
            nx::utils::random::number<float>(0, 1),
            nx::utils::random::number<float>(0, 1),
            nx::utils::random::number<float>(0, 1),
            nx::utils::random::number<float>(0, 1));
    }

    // TODO: requiredAttributes;

    if (nx::utils::random::number<bool>())
    {
        filter.freeText = attributeDictionary
            ? attributeDictionary->getRandomText()
            : generateFreeText();
    }

    filter.sortOrder = nx::utils::random::number<bool>()
        ? Qt::SortOrder::AscendingOrder
        : Qt::SortOrder::DescendingOrder;

    return filter;
}

common::metadata::DetectionMetadataPacketPtr generateRandomPacket(
    int eventCount,
    const AttributeDictionary* attributeDictionary)
{
    const int minAttributeCount = 0;
    const int maxAttributeCount = 7;

    auto packet = std::make_shared<common::metadata::DetectionMetadataPacket>();
    packet->deviceId = QnUuid::createUuid();
    packet->timestampUsec = nx::utils::random::number<qint64>();
    packet->durationUsec = nx::utils::random::number<qint64>(0, 30000);

    for (int i = 0; i < eventCount; ++i)
    {
        common::metadata::DetectedObject detectedObject;
        detectedObject.objectTypeId = QnUuid::createUuid();
        detectedObject.objectId = QnUuid::createUuid();
        detectedObject.boundingBox.setTopLeft(QPointF(
            nx::utils::random::number<float>(0, 1),
            nx::utils::random::number<float>(0, 1)));
        detectedObject.boundingBox.setWidth(nx::utils::random::number<float>(
            0,
            1 - detectedObject.boundingBox.topLeft().x()));
        detectedObject.boundingBox.setHeight(nx::utils::random::number<float>(
            0,
            1 - detectedObject.boundingBox.topLeft().y()));
        detectedObject.labels.resize(nx::utils::random::number<int>(
            minAttributeCount, maxAttributeCount));
        for (auto& attribute: detectedObject.labels)
        {
            attribute = attributeDictionary
                ? attributeDictionary->getRandomAttribute()
                : common::metadata::Attribute{
                    QString::fromUtf8(nx::utils::random::generateName(7)),
                    QString::fromUtf8(nx::utils::random::generateName(7))};
        }
        packet->objects.push_back(std::move(detectedObject));
    }

    return packet;
}

} // namespace test
} // namespace storage
} // namespace analytics
} // namespace nx
