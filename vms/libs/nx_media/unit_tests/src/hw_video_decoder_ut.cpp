// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <QtGui/QImage>

#include <nx/media/ffmpeg/demuxer.h>
#include <nx/media/ffmpeg/hw_video_decoder.h>
#include <nx/media/ffmpeg/io_context.h>
#include <nx/utils/log/log.h>

#if 0
// This is sample fast testing of hardware decoder, not a unit test

TEST(FfmpegHwVideoDecoderTest, SampleDecode)
{
    auto io = nx::media::ffmpeg::openFile("/home/leonid/videos/big_buck_bunny.mp4");
    ASSERT_TRUE(io);
    nx::media::ffmpeg::Demuxer demuxer;
    ASSERT_TRUE(demuxer.open(std::move(io)));

    auto decoder = std::make_unique<nx::media::ffmpeg::HwVideoDecoder>(
        AV_HWDEVICE_TYPE_VAAPI, nullptr);
    int i = 0;
    while(i++ < 25)
    {
        auto packet = demuxer.getNextData();
        if (!packet)
        {
            NX_DEBUG(this, "End of stream");
            return;
        }
        auto videoPacket = std::dynamic_pointer_cast<const QnCompressedVideoData>(packet);
        if (!videoPacket)
            continue;

        NX_DEBUG(this, "Decode packet %1x%2", videoPacket->width, videoPacket->height);
        CLVideoDecoderOutputPtr decodedFrame(new CLVideoDecoderOutput());
        if (decoder->decode(videoPacket, &decodedFrame) < 0)
            NX_ERROR(this, "Decode error");

        CLVideoDecoderOutputPtr softwareDecodedFrame(new CLVideoDecoderOutput());
        if (decodedFrame && decodedFrame->memoryType() == MemoryType::VideoMemory)
            softwareDecodedFrame = decodedFrame->toSystemMemory();
        else
            softwareDecodedFrame = decodedFrame;

        if (softwareDecodedFrame)
        {
            auto image = softwareDecodedFrame->toImage();
            image.save(QString::number(i) + QString("_frame.jpg"));
        }
    }
}

#endif
