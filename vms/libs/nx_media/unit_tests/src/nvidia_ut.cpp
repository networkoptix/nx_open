// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#if 0 //This code is only for a fast debugging decoder without a desktop client rendering framework.

#include <nx/utils/log/log.h>
#include <utils/media/io_context.h>
#include <nx/media/ffmpeg/demuxer.h>
#include <nx/media/nvidia/nvidia_video_decoder.h>
#include <nx/media/nvidia/nvidia_video_frame.h>
#include <nx/media/quick_sync/quick_sync_video_decoder.h>
#include <nx/media/hardware_acceleration_type.h>

TEST(NvidiaVideoDecoderTest, SampleDecode)
{
    auto io = nx::media::ffmpeg::openFile("/home/lbusygin/test.mkv");
    ASSERT_TRUE(io);
    nx::media::ffmpeg::Demuxer demuxer;
    ASSERT_TRUE(demuxer.open(std::move(io)));

    auto decoder = std::make_shared<nx::media::nvidia::NvidiaVideoDecoder>();
    while(true)
    {
        auto packet = demuxer.getNextData();
        if (!packet)
        {
            NX_ERROR(this, "End of stream");
            return;
        }
        auto videoPacket = std::dynamic_pointer_cast<const QnCompressedVideoData>(packet);
        if (!videoPacket)
            continue;

        NX_ERROR(this, "Decode packet");
        decoder->decode(videoPacket);
        auto frame = decoder->getFrame();
    }
}

TEST(NvidiaVideoDecoderTest, TestIsAvailable)
{
    auto hwtype = nx::media::getHardwareAccelerationType();
    std::cout << (int)hwtype << std::endl;

    if (nx::media::quick_sync::QuickSyncVideoDecoder::isAvailable())
        printf("quick_sync\n");
    if (nx::media::nvidia::NvidiaVideoDecoder::isAvailable())
        printf("nvidia\n");
}

#endif