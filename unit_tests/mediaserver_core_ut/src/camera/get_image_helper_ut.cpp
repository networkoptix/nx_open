#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <ostream>

#include <camera/get_image_helper.h>
#include <core/resource/camera_resource.h>
#include <utils/media/frame_info.h>

using ::testing::AtLeast;
using ::testing::Return;
using AspectRatio = ::nx::api::ImageRequest::AspectRatio;

namespace {

std::ostream& operator <<(std::ostream& stream, const QSize& size) {
    return stream << toString(size).toStdString();
}

} // namespace

namespace nx::test {

class MockCameraResource: public QnVirtualCameraResource
{
public:
    MOCK_CONST_METHOD0(getDriverName, QString());
    MOCK_CONST_METHOD0(aspectRatio, QnAspectRatio());

protected:
    MOCK_METHOD0(createLiveDataProvider, QnAbstractStreamDataProvider*());
};

TEST(GetImageHelper, updateDstSize_sourceRatio)
{
    MockCameraResource camera;
    CLVideoDecoderOutput outFrame;
    outFrame.width = 1920 / 2;
    outFrame.height = 1080 / 2;
    outFrame.sample_aspect_ratio = 1;

    EXPECT_EQ(QSize(1920, 1080), updateDstSize(camera, {1920, 1080}, outFrame, AspectRatio::source));
    EXPECT_EQ(QSize(1920, 1080), updateDstSize(camera, {0, 1080}, outFrame, AspectRatio::source));
    EXPECT_EQ(QSize(1920, 1080), updateDstSize(camera, {1920, 0}, outFrame, AspectRatio::source));
    EXPECT_EQ(QSize(outFrame.width, outFrame.height),
        updateDstSize(camera, {0, 0}, outFrame, AspectRatio::source));

    outFrame.sample_aspect_ratio = 2;
    EXPECT_EQ(QSize(1920, 540), updateDstSize(camera, {0, 0}, outFrame, AspectRatio::source));
}

TEST(GetImageHelper, updateDstSize_autoRatio)
{
    CLVideoDecoderOutput outFrame;
    outFrame.width = 1024 / 2;
    outFrame.height = 768 / 2;
    outFrame.sample_aspect_ratio = 1;

    MockCameraResource camera;
    EXPECT_CALL(camera, aspectRatio())
        .Times(AtLeast(1))
        .WillRepeatedly(Return(QnAspectRatio(1920, 1080)));

    EXPECT_EQ(QSize(1376, 768), updateDstSize(camera, {1024, 768}, outFrame, AspectRatio::auto_));
}

TEST(GetImageHelper, updateDstSize_minSize)
{
    static constexpr int kMinSize = nx::api::CameraImageRequest::kMinimumSize;

    MockCameraResource camera;
    CLVideoDecoderOutput outFrame;
    outFrame.width = kMinSize / 2;
    outFrame.height = kMinSize / 2;
    outFrame.sample_aspect_ratio = 1;

    // Width and height specified.
    EXPECT_EQ(QSize(kMinSize, kMinSize),
        updateDstSize(camera, {kMinSize / 2, kMinSize / 2}, outFrame, AspectRatio::source));

    // Width and height specified (persisting aspect ratio).
    EXPECT_EQ(QSize(kMinSize * 2, kMinSize),
        updateDstSize(camera, {kMinSize / 2, kMinSize / 4}, outFrame, AspectRatio::source));

    // Width specified.
    EXPECT_EQ(QSize(kMinSize, kMinSize),
        updateDstSize(camera, {0, kMinSize / 2}, outFrame, AspectRatio::source));

    // Nothing specified, should use source.
    EXPECT_EQ(QSize(kMinSize, kMinSize),
        updateDstSize(camera, {0, 0}, outFrame, AspectRatio::source));
}

TEST(GetImageHelper, updateDstSize_maxSize)
{
    static constexpr int kMaxSize = nx::api::CameraImageRequest::kMaximumSize;

    MockCameraResource camera;
    CLVideoDecoderOutput outFrame;
    outFrame.width = kMaxSize * 2;
    outFrame.height = kMaxSize * 2;
    outFrame.sample_aspect_ratio = 1;

    // Width and height specified.
    EXPECT_EQ(QSize(kMaxSize, kMaxSize),
        updateDstSize(camera, {kMaxSize * 2, kMaxSize * 2}, outFrame, AspectRatio::source));

    // Width and height specified (persisting aspect ratio).
    EXPECT_EQ(QSize(kMaxSize / 2, kMaxSize),
        updateDstSize(camera, {kMaxSize * 2, kMaxSize * 4}, outFrame, AspectRatio::source));

    // Width specified.
    EXPECT_EQ(QSize(kMaxSize, kMaxSize),
        updateDstSize(camera, {0, kMaxSize * 2}, outFrame, AspectRatio::source));

    // Nothing specified, should use source.
    EXPECT_EQ(QSize(kMaxSize, kMaxSize),
        updateDstSize(camera, {0, 0}, outFrame, AspectRatio::source));
}


} // namespace nx::test
