// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/media/stream_event.h>

namespace nx::media::test {

TEST(StreamEvent, serialization)
{
    StreamEventPacket event1;
    event1.code = StreamEvent::cannotDecryptMedia;
    unsigned char binaryData[256];
    for (int i = 0; i < 256; ++i)
        binaryData[i] = i;
    event1.extraData = QByteArray::fromRawData((char*) binaryData, 256);
    auto serializedData = serialize(event1);

    StreamEventPacket event2;
    deserialize(serializedData, &event2);
    ASSERT_EQ(event1, event2);
    ASSERT_EQ(256, event2.extraData.size());

    // Check compatibility with previous version.
    StreamEventPacket event3;
    serializedData = serializedData.split(';')[0];
    deserialize(serializedData, &event3);
    ASSERT_EQ(event1.code, event3.code);
    ASSERT_EQ(0, event3.extraData.size());
}

} // namespace nx::media::test
