#include "attribute_dictionary.h"

#include <algorithm>

#include <QtCore/QStringList>

#include <nx/utils/random.h>

namespace nx::analytics::db::test {

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
        filter.deviceIds.push_back(QnUuid::createUuid());

    filter.objectTypeId.resize(nx::utils::random::number<int>(0, 5));
    std::generate(
        filter.objectTypeId.begin(), filter.objectTypeId.end(),
        []() { return QnUuid::createUuid().toString(); });

    if (nx::utils::random::number<bool>())
        filter.objectTrackId = QnUuid::createUuid();

    // TODO: timePeriod.

    if (nx::utils::random::number<bool>())
    {
        filter.boundingBox = QRectF(
            nx::utils::random::number<float>(0, 1),
            nx::utils::random::number<float>(0, 1),
            nx::utils::random::number<float>(0, 1),
            nx::utils::random::number<float>(0, 1));
    }

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

static std::vector<common::metadata::Attribute> getUniqueAttributes(
    std::vector<common::metadata::Attribute> attributes)
{
    std::vector<common::metadata::Attribute> result;
    std::map<QString /*name*/, std::size_t /*position*/> uniqueAttributes;

    for (std::size_t i = 0; i < attributes.size(); ++i)
    {
        if (uniqueAttributes.count(attributes.at(i).name) == 0)
        {
            result.push_back(attributes.at(i));
            uniqueAttributes[attributes.at(i).name] = result.size() - 1;
        }
        else
        {
            result[uniqueAttributes[attributes.at(i).name]].value = attributes.at(i).value;
        }
    }

    return result;
}

common::metadata::ObjectMetadataPacketPtr generateRandomPacket(
    int objectCount,
    const AttributeDictionary* attributeDictionary)
{
    const int minAttributeCount = 0;
    const int maxAttributeCount = 7;

    auto packet = std::make_shared<common::metadata::ObjectMetadataPacket>();
    packet->deviceId = QnUuid::createUuid();
    packet->timestampUs = (nx::utils::random::number<qint64>() / 1000) * 1000;
    packet->durationUs = nx::utils::random::number<qint64>(0, 30) * 1000;

    for (int i = 0; i < objectCount; ++i)
    {
        common::metadata::ObjectMetadata objectMetadata;
        objectMetadata.typeId = QnUuid::createUuid().toString();
        objectMetadata.trackId = QnUuid::createUuid();
        objectMetadata.boundingBox = generateRandomRectf();
        objectMetadata.attributes.resize(nx::utils::random::number<int>(
            minAttributeCount, maxAttributeCount));
        for (auto& attribute: objectMetadata.attributes)
        {
            attribute = attributeDictionary
                ? attributeDictionary->getRandomAttribute()
                : common::metadata::Attribute{
                    QString::fromUtf8(nx::utils::random::generateName(7)),
                    QString::fromUtf8(nx::utils::random::generateName(7))};
        }

        objectMetadata.attributes = getUniqueAttributes(
            std::exchange(objectMetadata.attributes, {}));

        packet->objectMetadataList.push_back(std::move(objectMetadata));
    }

    return packet;
}

QRectF generateRandomRectf()
{
    QRectF rect;
    // NOTE: Leaving some space to the right & down so that the rect is not empty.
    rect.setTopLeft(QPointF(
        nx::utils::random::number<float>(0.0F, 0.95F),
        nx::utils::random::number<float>(0.0F, 0.95F)));
    rect.setWidth(nx::utils::random::number<float>(0.0F, 1 - rect.topLeft().x()));
    rect.setHeight(nx::utils::random::number<float>(0.0F, 1 - rect.topLeft().y()));
    return rect;
}

} // namespace nx::analytics::db::test
