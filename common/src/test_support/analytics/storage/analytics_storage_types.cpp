#include "analytics_storage_types.h"

#include <algorithm>

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

Filter generateRandomFilter()
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
        filter.freeText = generateFreeText();

    filter.sortOrder = nx::utils::random::number<bool>()
        ? Qt::SortOrder::AscendingOrder
        : Qt::SortOrder::DescendingOrder;

    return filter;
}

common::metadata::DetectionMetadataPacketPtr generateRandomPacket(int eventCount)
{
    auto packet = std::make_shared<common::metadata::DetectionMetadataPacket>();
    packet->deviceId = QnUuid::createUuid();
    packet->timestampUsec = nx::utils::random::number<qint64>();
    packet->durationUsec = nx::utils::random::number<qint64>(0, 30000);

    for (int i = 0; i < eventCount; ++i)
    {
        common::metadata::DetectedObject detectedObject;
        detectedObject.objectTypeId = QnUuid::createUuid();
        detectedObject.objectId = QnUuid::createUuid();
        detectedObject.boundingBox = QRectF(0, 0, 100, 100);
        detectedObject.labels.push_back(common::metadata::Attribute{
            QString::fromUtf8(nx::utils::random::generateName(7)),
            QString::fromUtf8(nx::utils::random::generateName(7))});
        packet->objects.push_back(std::move(detectedObject));
    }

    return packet;
}

} // namespace test
} // namespace storage
} // namespace analytics
} // namespace nx
