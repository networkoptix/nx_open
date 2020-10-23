#include <gtest/gtest.h>

#include <nx/streaming/video_data_packet.h>
#include <media/filters/h264_mp4_to_annexb.h>

TEST(mediaFilters, H2645Mp4ToAnnexB)
{
    // Check crash on invalid data.
    H2645Mp4ToAnnexB filter;
    filter.processData(nullptr);

    QnWritableCompressedVideoDataPtr result(new QnWritableCompressedVideoData());
    result->compressionType = AV_CODEC_ID_H264;
    filter.processData(result);
}

