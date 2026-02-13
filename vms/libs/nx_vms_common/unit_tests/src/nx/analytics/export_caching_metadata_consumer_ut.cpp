// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <analytics/common/object_metadata.h>
#include <nx/analytics/export_caching_metadata_consumer.h>
#include <nx/analytics/metadata_cache.h>
#include <nx/fusion/serialization/ubjson.h>

namespace nx::analytics {

namespace test {

using namespace std::chrono;

QnConstMetaDataV1Ptr prepareMotionData(
    int chanel,
    std::chrono::microseconds timestamp,
    int motionX,
    int motionY)
{
    QnMetaDataV1 data(timestamp, 0, 0);
    data.setMotionAt(motionX, motionY);
    data.channelNumber = chanel;
    return std::make_shared<const QnMetaDataV1>(data);
}

std::shared_ptr<const QnCompressedMetadata> generateObjectMetadata(
    const nx::common::metadata::ObjectMetadataPacket& packet,
    int channelNumber)
{
    QnCompressedMetadata metadata(MetadataType::ObjectDetection);
    metadata.setData(QnUbjson::serialized<nx::common::metadata::ObjectMetadataPacket>(packet));
    metadata.channelNumber = channelNumber;
    metadata.setTimestampUsec(packet.timestampUs);
    metadata.setDurationUsec(packet.durationUs);
    return std::make_shared<const QnCompressedMetadata>(metadata);
}

void motionsEQ(const QnConstMetaDataV1Ptr& data, const QnConstMetaDataV1Ptr& recievedData)
{
    ASSERT_EQ(data->isMotionAt(2, 3), recievedData->isMotionAt(2, 3));
    ASSERT_EQ(data->channelNumber, recievedData->channelNumber);
    ASSERT_EQ(data->metadataType, recievedData->metadataType);
    ASSERT_EQ(data->timestamp, recievedData->timestamp);
}

common::metadata::ObjectMetadata prepareObject(
    const QString& typeId,
    nx::Uuid uuid,
    QRect boundingBox)
{
    common::metadata::ObjectMetadata object;
    object.typeId = typeId;
    object.trackId = uuid;
    object.boundingBox = boundingBox;

    return object;
}

QnConstAbstractCompressedMetadataPtr compressedObjectPacket(
    const common::metadata::ObjectMetadata& object,
    qint64 timestampMs,
    size_t channel,
    qint64 duration = -1)
{
    common::metadata::ObjectMetadataPacket source_packet;
    source_packet.timestampUs = timestampMs * 1000;
    source_packet.durationUs = duration;
    source_packet.objectMetadataList.push_back(object);
    return generateObjectMetadata(source_packet, channel);
}

ExportCachingMetadataConsumer<nx::common::metadata::ObjectMetadataPacketPtr>
    createObjectMetadataCachingConsumer()
{
    return ExportCachingMetadataConsumer<nx::common::metadata::ObjectMetadataPacketPtr>(
        MetadataType::ObjectDetection, 50ms);
}

TEST(ExportCachingMetadataConsumer, simpleInAndOutMotion)
{
    ExportCachingMetadataConsumer<QnConstMetaDataV1Ptr>
        motionCache(MetadataType::Motion, 2s);
    ASSERT_EQ(motionCache.cacheDuration(), 2s);
    ASSERT_TRUE(motionCache.metadataType() == MetadataType::Motion);

    auto data = prepareMotionData(0, 111111us, 2, 3);
    motionCache.processMetadata(data);
    auto recievedData = motionCache.metadata(112ms, 0);
    ASSERT_EQ(recievedData.size(), 1);
    motionsEQ(data, recievedData[0]);
}

TEST(ExportCachingMetadataConsumer, simpleInAndOutObjectDetection)
{
    auto objectCache = createObjectMetadataCachingConsumer();
    ASSERT_EQ(objectCache.cacheDuration(), 50ms);
    ASSERT_TRUE(objectCache.metadataType() == MetadataType::ObjectDetection);

    auto object = prepareObject(
        "test",
        nx::Uuid("00000000-0000-0000-0000-000000000000"),
        {1, 2, 3, 4});
    objectCache.processMetadata(compressedObjectPacket(object, 1000, 0));

    auto recievedObjectData = objectCache.metadata(1040ms, 0);
    ASSERT_EQ(recievedObjectData.size(), 1);

    ASSERT_EQ(recievedObjectData[0]->objectMetadataList.size(), 1);
    ASSERT_EQ(recievedObjectData[0]->objectMetadataList.front(), object);
}

TEST(ExportCachingMetadataConsumer, differentChannelsAreIndependent)
{
    auto objectCache = createObjectMetadataCachingConsumer();
    auto object1 = prepareObject(
        "chanel0_object0",
        nx::Uuid("00000000-0000-0000-0000-000000000001"),
        {1, 2, 3, 4});
    objectCache.processMetadata(compressedObjectPacket(object1, 1000, 0));

    auto object2 = prepareObject(
        "chanel1_object0",
        nx::Uuid("00000000-0000-0000-0000-000000000001"),
        {1, 2, 3, 4});
    objectCache.processMetadata(compressedObjectPacket(object2, 1010, 1));

    auto object3 = prepareObject(
        "chanel1_object1",
        nx::Uuid("00000000-0000-0000-0000-000000000002"),
        {3, 2, 3, 4});
    objectCache.processMetadata(compressedObjectPacket(object3, 1020, 1));

    auto recievedObjectData = objectCache.metadata(1040ms, 0);
    ASSERT_EQ(recievedObjectData.size(), 1);
    ASSERT_EQ(recievedObjectData[0]->objectMetadataList.size(), 1);
    ASSERT_EQ(recievedObjectData[0]->objectMetadataList.front(), object1);
    ASSERT_EQ(recievedObjectData[0]->timestampUs, 1000 * 1000);

    auto recievedObjectData2 = objectCache.metadata(1040ms, 1);
    ASSERT_EQ(recievedObjectData2.size(), 2);
    ASSERT_EQ(recievedObjectData2[0]->objectMetadataList.front(), object3);
    ASSERT_EQ(recievedObjectData2[0]->timestampUs, 1020 * 1000);
    ASSERT_EQ(recievedObjectData2[1]->objectMetadataList.front(), object2);
    ASSERT_EQ(recievedObjectData2[1]->timestampUs, 1010 * 1000);
}

TEST(ExportCachingMetadataConsumer, removesOldMetadataWhenCacheOverflow)
{
    auto objectCache = createObjectMetadataCachingConsumer();

    auto object1 = prepareObject(
        "object1",
        nx::Uuid("00000000-0000-0000-0000-000000000001"),
        {1, 2, 3, 4});
    objectCache.processMetadata(compressedObjectPacket(object1, 1000, 0));

    auto object2 = prepareObject(
        "object2",
        nx::Uuid("00000000-0000-0000-0000-000000000002"),
        {5, 2, 3, 4});
    objectCache.processMetadata(compressedObjectPacket(object2, 1030, 0));

    auto object3 = prepareObject(
        "object3",
        nx::Uuid("00000000-0000-0000-0000-000000000003"),
        {3, 2, 3, 4});
    objectCache.processMetadata(compressedObjectPacket(object3, 1060, 0));

    auto recievedObjectData = objectCache.metadata(1090ms, 0);
    ASSERT_EQ(recievedObjectData.size(), 2);
    ASSERT_EQ(recievedObjectData[0]->objectMetadataList.front(), object3);
    ASSERT_EQ(recievedObjectData[0]->timestampUs, 1060 * 1000);
    ASSERT_EQ(recievedObjectData[1]->objectMetadataList.front(), object2);
    ASSERT_EQ(recievedObjectData[1]->timestampUs, 1030 * 1000);
}

TEST(ExportCachingMetadataConsumer, removesOldMetadataWhenCacheOverflowInSameTrack)
{
    auto objectCache = createObjectMetadataCachingConsumer();

    auto object1 = prepareObject(
        "object1",
        nx::Uuid("00000000-0000-0000-0000-000000000001"),
        {1, 2, 3, 4});
    objectCache.processMetadata(compressedObjectPacket(object1, 1000, 0));

    auto object2 = prepareObject(
        "object1",
        nx::Uuid("00000000-0000-0000-0000-000000000001"),
        {5, 2, 3, 4});
    objectCache.processMetadata(compressedObjectPacket(object2, 1030, 0));

    auto object3 = prepareObject(
        "object1",
        nx::Uuid("00000000-0000-0000-0000-000000000001"),
        {3, 2, 3, 4});
    objectCache.processMetadata(compressedObjectPacket(object3, 1060, 0));

    auto recievedObjectData = objectCache.metadata(1090ms, 0);
    ASSERT_EQ(recievedObjectData.size(), 2);
    ASSERT_EQ(recievedObjectData[0]->objectMetadataList.front(), object3);
    ASSERT_EQ(recievedObjectData[0]->timestampUs, 1060 * 1000);
    ASSERT_EQ(recievedObjectData[1]->objectMetadataList.front(), object2);
    ASSERT_EQ(recievedObjectData[1]->timestampUs, 1030 * 1000);
}

TEST(ExportCachingMetadataConsumer, objectGatheredCorrectly)
{
    auto objectCache = createObjectMetadataCachingConsumer();

    auto object1 = prepareObject(
        "object1",
        nx::Uuid("00000000-0000-0000-0000-000000000001"),
        {1, 2, 3, 4});
    objectCache.processMetadata(compressedObjectPacket(object1, 1000, 0));

    auto object2 = prepareObject(
        "object1",
        nx::Uuid("00000000-0000-0000-0000-000000000001"),
        {5, 2, 3, 4});
    objectCache.processMetadata(compressedObjectPacket(object2, 1010, 0));

    auto recievedObjectData = objectCache.metadata(1050ms, 0);
    ASSERT_EQ(recievedObjectData.size(), 2);
    ASSERT_EQ(recievedObjectData[0]->objectMetadataList.front(), object2);
    ASSERT_EQ(recievedObjectData[0]->timestampUs, 1010 * 1000);
    ASSERT_EQ(recievedObjectData[1]->objectMetadataList.front(), object1);
    ASSERT_EQ(recievedObjectData[1]->timestampUs, 1000 * 1000);
}

TEST(ExportCachingMetadataConsumer, providedDurations)
{
    auto objectCache = createObjectMetadataCachingConsumer();

    auto object1 = prepareObject(
        "object1",
        nx::Uuid("00000000-0000-0000-0000-000000000001"),
        {1, 2, 3, 4});
    objectCache.processMetadata(compressedObjectPacket(object1, 1000, 0, /*duration*/ 500000));

    auto object2 = prepareObject(
        "object2",
        nx::Uuid("00000000-0000-0000-0000-000000000002"),
        {5, 2, 3, 4});
    objectCache.processMetadata(compressedObjectPacket(object2, 1010, 0));

    auto recievedObjectData = objectCache.metadata(1105ms, 0);
    ASSERT_EQ(recievedObjectData.size(), 2);
    ASSERT_EQ(recievedObjectData[0]->objectMetadataList.front(), object2);
    ASSERT_EQ(recievedObjectData[0]->timestampUs, 1010 * 1000);
    ASSERT_EQ(recievedObjectData[1]->objectMetadataList.front(), object1);
    ASSERT_EQ(recievedObjectData[1]->timestampUs, 1000 * 1000);

    auto recievedObjectData2 = objectCache.metadata(1200ms, 0);
    ASSERT_EQ(recievedObjectData2.size(), 1);
    ASSERT_EQ(recievedObjectData2[0]->objectMetadataList.front(), object1);
    ASSERT_EQ(recievedObjectData2[0]->timestampUs, 1000 * 1000);
}

TEST(ExportCachingMetadataConsumer, checkFromTheMiddleOfCache)
{
    auto objectCache = createObjectMetadataCachingConsumer();
    auto object1 = prepareObject(
        "object1",
        nx::Uuid("00000000-0000-0000-0000-000000000001"),
        {1, 2, 3, 4});
    objectCache.processMetadata(compressedObjectPacket(object1, 1000, 0));

    auto object2 = prepareObject(
        "chanel1",
        nx::Uuid("00000000-0000-0000-0000-000000000001"),
        {1, 2, 3, 4});
    objectCache.processMetadata(compressedObjectPacket(object2, 1030, 0));

    auto recievedObjectData = objectCache.metadata(1040ms, 0);
    ASSERT_EQ(recievedObjectData.size(), 2);
    ASSERT_EQ(recievedObjectData[0]->objectMetadataList.front(), object2);
    ASSERT_EQ(recievedObjectData[0]->timestampUs, 1030 * 1000);
    ASSERT_EQ(recievedObjectData[1]->objectMetadataList.front(), object1);
    ASSERT_EQ(recievedObjectData[1]->timestampUs, 1000 * 1000);

    auto object3 = prepareObject(
        "chanel1_object1",
        nx::Uuid("00000000-0000-0000-0000-000000000002"),
        {3, 2, 3, 4});
    objectCache.processMetadata(compressedObjectPacket(object3, 1070, 0));

    auto recievedObjectData2 = objectCache.metadata(1040ms, 0);
    ASSERT_EQ(recievedObjectData2.size(), 1);
    ASSERT_EQ(recievedObjectData2[0]->objectMetadataList.front(), object2);
    ASSERT_EQ(recievedObjectData2[0]->timestampUs, 1030 * 1000);

    auto recievedObjectData3 = objectCache.metadata(1080ms, 0);
    ASSERT_EQ(recievedObjectData3.size(), 2);
    ASSERT_EQ(recievedObjectData3[0]->objectMetadataList.front(), object3);
    ASSERT_EQ(recievedObjectData3[0]->timestampUs, 1070 * 1000);
    ASSERT_EQ(recievedObjectData3[1]->objectMetadataList.front(), object2);
    ASSERT_EQ(recievedObjectData3[1]->timestampUs, 1030 * 1000);

    objectCache.processMetadata(compressedObjectPacket(object1, 1060, 0));
    auto recievedObjectData4 = objectCache.metadata(1080ms, 0);
    ASSERT_EQ(recievedObjectData4.size(), 3);
    ASSERT_EQ(recievedObjectData4[0]->objectMetadataList.front(), object3);
    ASSERT_EQ(recievedObjectData4[0]->timestampUs, 1070 * 1000);
    ASSERT_EQ(recievedObjectData4[1]->objectMetadataList.front(), object1);
    ASSERT_EQ(recievedObjectData4[1]->timestampUs, 1060 * 1000);
    ASSERT_EQ(recievedObjectData4[2]->objectMetadataList.front(), object2);
    ASSERT_EQ(recievedObjectData4[2]->timestampUs, 1030 * 1000);
}

} // namespace test
} // namespace nx::analytics
