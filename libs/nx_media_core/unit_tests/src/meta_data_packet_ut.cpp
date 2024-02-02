// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/media/meta_data_packet.h>

namespace nx::media::test {

TEST(MetaData, serialization)
{
    QnMetaDataV1 packet;
    packet.metadataType = MetadataType::Motion;
    packet.m_duration = 5000;
    packet.timestamp = 12345678000; // Serializer will truncate microseconds part.

    // Serialize.
    auto data = packet.serialize();

    // Deserilize.
    QnMetaDataV1Light motionData;
    ASSERT_EQ(data.size(), sizeof(motionData));
    memcpy(&motionData, data.data(), sizeof(motionData));
    motionData.doMarshalling();
    auto packetCopy = QnMetaDataV1::fromLightData(motionData);

    ASSERT_EQ(packet.m_duration, packetCopy->m_duration);
    ASSERT_EQ(packet.timestamp, packetCopy->timestamp);
}

} // namespace nx::media::test
