#include <gtest/gtest.h>

#include <QBuffer>
#include <QImage>

#include <camera/get_image_helper.h>
#include <motion/motion_estimation.h>
#include <media_server/media_server_module.h>

TEST(MotionEstimation, Analyze)
{
    QnMediaServerModule module;
    QnGetImageHelper helper(&module);
    QnMotionEstimation estimation({}, nullptr);
    estimation.setMotionMask({});
    auto frame = std::make_shared<QnWritableCompressedVideoData>();
    frame->flags = QnAbstractMediaData::MediaFlags_AVKey;
    frame->compressionType = AV_CODEC_ID_MJPEG;
    const int resolutions = 12;
    const int widths[resolutions] = {
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
    const int heights[resolutions] = {
        64, 80, 96, 128, 256, 512, 576, 1024, 720, 2048, 4096, 8192};
    for (int resolution = 0; resolution < resolutions; ++resolution)
    {
        const int width = widths[resolution];
        const int height = heights[resolution];
        frame->width = width;
        frame->height = height;
        {
            QImage image(width, height, QImage::Format_RGBA8888);
            CLVideoDecoderOutputPtr output(new CLVideoDecoderOutput(image));
            QByteArray data = helper.encodeImage(output, "jpeg");
            frame->m_data.resize(data.size());
            memcpy(frame->m_data.data(), data.data(), data.size());
        }
        ASSERT_TRUE(estimation.analyzeFrame(frame));
        ASSERT_TRUE(estimation.analyzeFrame(frame));
    }
}
