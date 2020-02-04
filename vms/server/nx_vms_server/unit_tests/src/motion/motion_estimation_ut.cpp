#include <gtest/gtest.h>

#include <QBuffer>
#include <QImage>

#include <camera/get_image_helper.h>
#include <motion/motion_estimation.h>
#include <media_server/media_server_module.h>

static const int resolutions = 12;
static const int widths[resolutions] = {
    64 + 5,
    80 + 11,
    96 + 7,
    128 + 3,
    256 + 2,
    512 + 1,
    720,
    1024 + 14,
    1280,
    2048 + 10,
    4096 + 9,
    8192};

static const int heights[resolutions] = {
    64 + 3,
    80 + 4,
    96 + 9,
    128 + 1,
    256 + 5,
    512 + 6,
    576 + 7,
    1024 + 11,
    720 + 2,
    2048 + 3,
    4096 + 5,
    8192 + 7};

namespace nx::vms::server::test {

TEST(MotionEstimation, Analyze)
{
    QnMediaServerModule module;
    QnGetImageHelper helper(&module);
    QnMotionEstimation estimation({}, nullptr);
    estimation.setMotionMask({});
    auto frame = std::make_shared<QnWritableCompressedVideoData>();
    frame->flags = QnAbstractMediaData::MediaFlags_AVKey;
    frame->compressionType = AV_CODEC_ID_MJPEG;
    frame->timestamp = 0;
    for (int resolution = 0; resolution < resolutions; ++resolution)
    {
        const int width = widths[resolution];
        const int height = heights[resolution];
        frame->width = width;
        frame->height = height;
        {
            CLVideoDecoderOutputPtr output(new CLVideoDecoderOutput);
            output->reallocate(width, height, AV_PIX_FMT_YUV420P, width);
            frame->m_data.write(helper.encodeImage(output, "jpeg"));
        }
        frame->timestamp += MOTION_AGGREGATION_PERIOD;
        ASSERT_TRUE(estimation.analyzeFrame(frame));
        frame->timestamp += MOTION_AGGREGATION_PERIOD;
        ASSERT_TRUE(estimation.analyzeFrame(frame));
        ASSERT_TRUE(estimation.tryToCreateMotionMetadata());
    }
}

} // namespace nx::vms::server::test
