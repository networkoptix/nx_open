// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <QtGui/QPainter>

#include <nx/analytics/taxonomy/abstract_state.h>
#include <nx/core/transcoding/filters/motion_filter.h>
#include <nx/fusion/serialization/ubjson.h>
#include <nx/media/ffmpeg/frame_info.h>
#include <nx/media/meta_data_packet.h>

namespace {

const QColor kNoPaint(0xffffff);
const QColor kForegroundColor(0xff0000);
const QColor kBackgroundColor(0x00ff00);

constexpr QRectF k_rectF(0.1, 0.1, 0.1, 0.1);
constexpr QRectF k_rectF2(0.3, 0.3, 0.1, 0.1);

} // namespace

class MotionFilterTest: public ::testing::Test
{
protected:
    MotionFilterTest() {}

    nx::core::transcoding::MotionExportSettings makeSettings()
    {
        return {
            .foreground = kForegroundColor,
            .background = kBackgroundColor};
    }

    QImage prepareImage()
    {
        QImage image(440, 320, QImage::Format_RGB32);
        image.fill(kNoPaint);
        return image;
    }
};

using namespace nx::core::transcoding;

TEST_F(MotionFilterTest, noMotion)
{
    MotionFilter filter(makeSettings());
    filter.setMetadata({});
    QImage image = prepareImage();
    filter.updateImage(image);

    ASSERT_NE(image, prepareImage());
    ASSERT_EQ(image.pixelColor(35, 25), kNoPaint);
    ASSERT_EQ(image.pixelColor(30, 20), kBackgroundColor);
}

TEST_F(MotionFilterTest, motionLongerThan3sec)
{
    MotionFilter filter(makeSettings());
    QnMetaDataV1Ptr metaDataPtr(new QnMetaDataV1());
    metaDataPtr->timestamp = std::chrono::system_clock::now().time_since_epoch().count() - 5000;
    filter.setMetadata({metaDataPtr});
    QImage image = prepareImage();
    filter.updateImage(image);

    ASSERT_NE(image, prepareImage());
    ASSERT_EQ(image.pixelColor(35, 25), kNoPaint);
    ASSERT_EQ(image.pixelColor(30, 20), kBackgroundColor);
    ASSERT_EQ(image.pixelColor(45, 30), kBackgroundColor);
    ASSERT_EQ(image.pixelColor(88, 30), kBackgroundColor);
}

TEST_F(MotionFilterTest, OneMotionPackage)
{
    MotionFilter filter(makeSettings());
    QnMetaDataV1Ptr metaDataPtr(new QnMetaDataV1());
    metaDataPtr->addMotion(k_rectF);
    QImage image = prepareImage();

    metaDataPtr->timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    filter.setMetadata({metaDataPtr});
    filter.updateImage(image);

    ASSERT_NE(image, prepareImage());
    ASSERT_EQ(image.pixelColor(35, 25), kNoPaint);
    ASSERT_EQ(image.pixelColor(30, 20), kBackgroundColor);
    ASSERT_EQ(image.pixelColor(45, 30), kForegroundColor);
    ASSERT_EQ(image.pixelColor(88, 30), kForegroundColor);
    ASSERT_EQ(image.pixelColor(95, 30), kBackgroundColor);
    ASSERT_EQ(image.pixelColor(60, 90), kBackgroundColor);
    ASSERT_EQ(image.pixelColor(50, 50), kForegroundColor);
    ASSERT_EQ(image.pixelColor(55, 55), kNoPaint);
}

TEST_F(MotionFilterTest, TwoMotionPackages)
{
    MotionFilter filter(makeSettings());
    QnMetaDataV1Ptr metaDataPtr1(new QnMetaDataV1());
    QnMetaDataV1Ptr metaDataPtr2(new QnMetaDataV1());

    metaDataPtr1->addMotion(k_rectF);
    metaDataPtr2->addMotion(k_rectF2);
    QImage image = prepareImage();

    metaDataPtr1->timestamp = std::chrono::system_clock::now().time_since_epoch().count() - 500;
    metaDataPtr2->timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    filter.setMetadata({metaDataPtr1, metaDataPtr2});
    filter.updateImage(image);

    ASSERT_NE(image, prepareImage());
    ASSERT_EQ(image.pixelColor(35, 25), kNoPaint);
    ASSERT_EQ(image.pixelColor(30, 20), kBackgroundColor);
    ASSERT_EQ(image.pixelColor(45, 30), kBackgroundColor);
    ASSERT_EQ(image.pixelColor(88, 30), kBackgroundColor);
    ASSERT_EQ(image.pixelColor(95, 30), kBackgroundColor);
    ASSERT_EQ(image.pixelColor(60, 90), kBackgroundColor);
    ASSERT_EQ(image.pixelColor(50, 50), kBackgroundColor);
    ASSERT_EQ(image.pixelColor(55, 55), kNoPaint);
    ASSERT_EQ(image.pixelColor(135, 130), kForegroundColor);
    ASSERT_EQ(image.pixelColor(180, 90), kForegroundColor);
}
