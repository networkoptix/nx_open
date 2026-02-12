// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/media/audio_data_packet.h>
#include <nx/media/video_data_packet.h>
#include <nx/webrtc/media_queue.h>

QnAbstractMediaDataPtr createVideoPacket(nx::Uuid deviceId, bool keyFrame)
{
    QnAbstractMediaDataPtr data(new QnWritableCompressedVideoData());
    data->deviceId = deviceId;
    if (keyFrame)
        data->flags |= QnAbstractMediaData::MediaFlags_AVKey;

    return data;
}

QnAbstractMediaDataPtr createAudioPacket(nx::Uuid deviceId)
{
    QnWritableCompressedAudioDataPtr data(new QnWritableCompressedAudioData());
    data->deviceId = deviceId;
    data->flags |= QnAbstractMediaData::MediaFlags_AVKey;

    return data;
}

TEST(MediaQueue, testDeviceId)
{
    nx::vms::server::MediaQueue queue(7);
    nx::Uuid deviceId1("11111111-1111-1111-1111-111111111111");
    nx::Uuid deviceId2("22222222-2222-2222-2222-222222222222");

    queue.putData(createVideoPacket(deviceId1, /*keyFrame*/ true));
    queue.putData(createAudioPacket(deviceId1));
    queue.putData(createVideoPacket(deviceId2, /*keyFrame*/ true));
    queue.putData(createAudioPacket(deviceId2));
    queue.putData(createVideoPacket(deviceId1, /*keyFrame*/ false));
    queue.putData(createVideoPacket(deviceId2, /*keyFrame*/ false));
    queue.putData(createVideoPacket(deviceId1, /*keyFrame*/ true));
    queue.putData(createVideoPacket(deviceId1, /*keyFrame*/ false));

    // We have to remove fisrt gop of first deivce and all aduio inside it, and leave as is
    // second device data.
    auto data = queue.popData(false);
    ASSERT_EQ(data->deviceId, deviceId2);
    ASSERT_TRUE(data->flags & AV_PKT_FLAG_KEY);

    data = queue.popData(false);
    ASSERT_EQ(data->deviceId, deviceId2);
    ASSERT_EQ(data->dataType, QnAbstractMediaData::AUDIO);

    data = queue.popData(false);
    ASSERT_EQ(data->deviceId, deviceId2);
    ASSERT_FALSE(data->flags & AV_PKT_FLAG_KEY);
}
