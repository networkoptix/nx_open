// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/utils/log/log.h>
#include <nx/media/ffmpeg/io_context.h>
#include <nx/media/ffmpeg/demuxer.h>
#include <nx/media/nvidia/nvidia_video_decoder.h>
#include <nx/media/nvidia/nvidia_video_frame.h>


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

        nx::QVideoFramePtr result;
        NX_ERROR(this, "Decode packet");
        decoder->decode(videoPacket, &result);
        auto frame = decoder->getFrame();
        if (frame)
            frame->renderToRgb(false, 1, 1, nullptr, nullptr);
    }
}