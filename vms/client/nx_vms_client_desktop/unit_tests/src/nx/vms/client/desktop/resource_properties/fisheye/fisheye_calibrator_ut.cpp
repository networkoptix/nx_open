// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <QtGui/QImage>
#include <QtGui/QPainter>

#include <nx/vms/client/desktop/resource_properties/fisheye/fisheye_calibrator.h>

namespace nx::vms::client::desktop {
namespace test {

namespace {

static constexpr int kPixelPrecision = 2.0;

QImage prepareImage(int width, int height, QImage::Format format, QColor backgroundColor,
    QColor foregroundColor, const QPointF& center, qreal radius, qreal stretch)
{
    QImage image(width, height, format);
    image.fill(backgroundColor);

    const QPointF pixCenter(center.x() * width, center.y() * height);
    const QPointF pixRadii(radius * width, radius * width / stretch);

    QPainter painter(&image);
    painter.setPen(Qt::NoPen);
    painter.setBrush(foregroundColor);
    painter.drawEllipse(pixCenter, pixRadii.x(), pixRadii.y());

    return image;
}

qreal getTolerance(const QSize& size)
{
    return qreal(kPixelPrecision) / qMin(size.width(), size.height());
}

qreal getStretchTolerance(const QSize& size)
{
    qreal width = size.width();
    qreal height = size.height();
    if (width < height)
        std::swap(width, height);

    return ((width + kPixelPrecision) / (height - kPixelPrecision)) - (width / height);
}

} // namespace

class FisheyeCalibratorTest: protected FisheyeCalibrator, public ::testing::Test
{
};

TEST_F(FisheyeCalibratorTest, circleInSquare)
{
    const QPointF center(0.5, 0.5);
    const qreal radius = 0.5;
    const qreal stretch = 1.0;

    const int size = 800;

    const auto image = prepareImage(size, size, QImage::Format_RGB32, Qt::black, Qt::white,
        center, radius, stretch);

    const auto result = analyzeFrame(image);
    ASSERT_EQ(result, FisheyeCalibrator::Result::ok);

    const qreal tolerance = getTolerance(image.size());
    EXPECT_NEAR(this->center().x(), center.x(), tolerance);
    EXPECT_NEAR(this->center().y(), center.y(), tolerance);
    EXPECT_NEAR(this->radius(), radius, tolerance);
    EXPECT_NEAR(this->stretch(), stretch, getStretchTolerance(image.size()));
}

TEST_F(FisheyeCalibratorTest, circleInRectangle)
{
    const int width = 800;
    const int height = 600;
    const qreal aspectRatio = qreal(width) / height;

    const QPointF center(0.5, 0.5);
    const qreal radius = 0.5 / aspectRatio;
    const qreal stretch = 1.0;

    const auto image = prepareImage(width, height, QImage::Format_RGB32, Qt::black, Qt::red,
        center, radius, stretch);

    const auto result = analyzeFrame(image);
    ASSERT_EQ(result, FisheyeCalibrator::Result::ok);

    const qreal tolerance = getTolerance(image.size());
    EXPECT_NEAR(this->center().x(), center.x(), tolerance);
    EXPECT_NEAR(this->center().y(), center.y(), tolerance);
    EXPECT_NEAR(this->radius(), radius, tolerance);
    EXPECT_NEAR(this->stretch(), stretch, getStretchTolerance(image.size()));
}

TEST_F(FisheyeCalibratorTest, clippedCircleInRectangle)
{
    const int width = 800;
    const int height = 600;
    const qreal aspectRatio = qreal(width) / height;

    const QPointF center(0.5, 0.5);
    const qreal radius = 0.6 / aspectRatio;
    const qreal stretch = 1.0;

    const auto image = prepareImage(width, height, QImage::Format_RGB32, Qt::black, Qt::green,
        center, radius, stretch);

    const auto result = analyzeFrame(image);
    ASSERT_EQ(result, FisheyeCalibrator::Result::ok);

    const qreal tolerance = getTolerance(image.size());
    EXPECT_NEAR(this->center().x(), center.x(), tolerance);
    EXPECT_NEAR(this->center().y(), center.y(), tolerance);
    EXPECT_NEAR(this->radius(), radius, tolerance);
    EXPECT_NEAR(this->stretch(), stretch, getStretchTolerance(image.size()));
}

TEST_F(FisheyeCalibratorTest, shiftedCircleInRectangle)
{
    const int width = 800;
    const int height = 600;
    const qreal aspectRatio = qreal(width) / height;

    const qreal radius = 0.5 / aspectRatio;
    const qreal stretch = 1.0;
    const QPointF center(radius, 0.5);

    const auto image = prepareImage(width, height, QImage::Format_RGB32, Qt::black, Qt::blue,
        center, radius, stretch);

    const auto result = analyzeFrame(image);
    ASSERT_EQ(result, FisheyeCalibrator::Result::ok);

    const qreal tolerance = getTolerance(image.size());
    EXPECT_NEAR(this->center().x(), center.x(), tolerance);
    EXPECT_NEAR(this->center().y(), center.y(), tolerance);
    EXPECT_NEAR(this->radius(), radius, tolerance);
    EXPECT_NEAR(this->stretch(), stretch, getStretchTolerance(image.size()));
}

TEST_F(FisheyeCalibratorTest, ellipse)
{
    const int width = 800;
    const int height = 600;
    const qreal aspectRatio = qreal(width) / height;

    const QPointF center(0.5, 0.5);
    const qreal radius = 0.5;
    const qreal stretch = aspectRatio;

    const auto image = prepareImage(width, height, QImage::Format_RGB32, 0x101010, Qt::white,
        center, radius, stretch);

    const auto result = analyzeFrame(image);
    ASSERT_EQ(result, FisheyeCalibrator::Result::ok);

    const qreal tolerance = getTolerance(image.size());
    EXPECT_NEAR(this->center().x(), center.x(), tolerance);
    EXPECT_NEAR(this->center().y(), center.y(), tolerance);
    EXPECT_NEAR(this->radius(), radius, tolerance);
    EXPECT_NEAR(this->stretch(), stretch, getStretchTolerance(image.size()));
}

TEST_F(FisheyeCalibratorTest, verticalEllipse)
{
    const int width = 600;
    const int height = 800;
    const qreal aspectRatio = qreal(width) / height;

    const QPointF center(0.5, 0.5);
    const qreal radius = 0.5;
    const qreal stretch = aspectRatio;

    const auto image = prepareImage(width, height, QImage::Format_RGB32, Qt::black, Qt::white,
        center, radius, stretch);

    const auto result = analyzeFrame(image);
    ASSERT_EQ(result, FisheyeCalibrator::Result::ok);

    // A larger tolerance is used here.
    // TODO: Improve the calibrator precision for rare cases of ellipse positioning.

    const qreal tolerance = getTolerance(image.size()) * 2;
    EXPECT_NEAR(this->center().x(), center.x(), tolerance);
    EXPECT_NEAR(this->center().y(), center.y(), tolerance);
    EXPECT_NEAR(this->radius(), radius, tolerance);
    EXPECT_NEAR(this->stretch(), stretch, getStretchTolerance(image.size()) * 2);
}

TEST_F(FisheyeCalibratorTest, arbitraryShiftedClippedEllipse)
{
    const int width = 800;
    const int height = 600;
    const qreal aspectRatio = qreal(width) / height;

    const QPointF pixCenter(390, 310);
    const QPointF pixRadii(420, 320);

    const QPointF center(pixCenter.x() / width, pixCenter.y() / height);
    const qreal radius = pixRadii.x() / width;
    const qreal stretch = pixRadii.x() / pixRadii.y();

    const auto image = prepareImage(width, height, QImage::Format_RGB32, Qt::black, Qt::white,
        center, radius, stretch);

    const auto result = analyzeFrame(image);
    ASSERT_EQ(result, FisheyeCalibrator::Result::ok);

    // A larger tolerance and a very large stretch tolerance is used here.
    // TODO: Improve the calibrator precision for rare cases of ellipse positioning.

    const qreal tolerance = getTolerance(image.size()) * 2;
    EXPECT_NEAR(this->center().x(), center.x(), tolerance);
    EXPECT_NEAR(this->center().y(), center.y(), tolerance);
    EXPECT_NEAR(this->radius(), radius, tolerance);
    EXPECT_NEAR(this->stretch(), stretch, 0.075);
}

TEST_F(FisheyeCalibratorTest, tooLowLight)
{
    const int size = 800;

    const auto image = prepareImage(size, size, QImage::Format_RGB32, Qt::black, 0x101010,
        {0.5, 0.5}, 0.5, 1.0);

    FisheyeCalibrator calibrator;
    const auto result = analyzeFrame(image);
    ASSERT_EQ(result, FisheyeCalibrator::Result::errorTooLowLight);
}

TEST_F(FisheyeCalibratorTest, notFisheyeImage)
{
    QImage image(800, 600, QImage::Format_RGB32);
    image.fill(Qt::black);
    {
        QPainter painter(&image);
        painter.fillRect(0, 0, 800, 600, QBrush(Qt::white, Qt::DiagCrossPattern));
    }

    FisheyeCalibrator calibrator;
    const auto result = analyzeFrame(image);
    ASSERT_EQ(result, FisheyeCalibrator::Result::errorNotFisheyeImage);
}

TEST_F(FisheyeCalibratorTest, notFisheyeImage_barrelDistorted)
{
    QImage image;
    image.load(":/fisheye_calibrator_ut/calibration_test.png");

    FisheyeCalibrator calibrator;
    const auto result = analyzeFrame(image);
    ASSERT_EQ(result, FisheyeCalibrator::Result::errorNotFisheyeImage);
}

} // namespace test
} // namespace nx::vms::client::desktop
