// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <QtGui/QOffscreenSurface>
#include <QtGui/QPainterPath>
#include <QtGui/rhi/qrhi.h>

#include <nx/pathkit/rhi_paint_device.h>
#include <nx/pathkit/rhi_paint_engine.h>
#include <nx/utils/test_support/test_with_temporary_directory.h>

#include "rhi_offscreen_renderer.h"

namespace nx::pathkit {

namespace test {

class PaintEngineTest:
    public ::testing::Test,
    public nx::utils::test::TestWithTemporaryDirectory
{
public:
    using TestFunc = std::function<void(QPaintDevice*)>;

public:
    virtual void SetUp() override
    {
        if (qgetenv("QT_QPA_PLATFORM").toStdString() == "offscreen")
        {
            QRhiInitParams params;
            rhi.reset(QRhi::create(QRhi::Null, &params));
            return;
        }

        QRhiGles2InitParams params;
        params.format = QSurfaceFormat::defaultFormat();
        params.fallbackSurface = QRhiGles2InitParams::newFallbackSurface();
        rhi.reset(QRhi::create(QRhi::OpenGLES2, &params));
    }

    virtual void TearDown() override
    {
    }

    void checkResult(QSize size, float pixelRatio, const QString& name, TestFunc testFunc);

public:
    std::unique_ptr<QRhi> rhi;
};

namespace {

QImage imageDiff(const QImage& img1, const QImage& img2, int& accError)
{
    if (img1.size() != img2.size())
    {
        accError = -1;
        return {};
    }

    accError = 0;

    QImage result(img1.size(), QImage::Format_RGBA8888_Premultiplied);
    result.fill(Qt::black);

    for (int y = 0; y < img1.height(); ++y)
    {
        for (int x = 0; x < img1.width(); ++x)
        {
            const QColor c1 = img1.pixelColor(x, y);
            const QColor c2 = img2.pixelColor(x, y);

            const int r = std::min(std::abs(c1.red() - c2.red()), 255);
            const int g = std::min(std::abs(c1.green() - c2.green()), 255);
            const int b = std::min(std::abs(c1.blue() - c2.blue()), 255);
            const int a = 255;

            accError += r + g + b;

            result.setPixel(x, y, qRgba(r, g, b, a));
        }
    }

    return result;
}

} // namespace

void PaintEngineTest::checkResult(
    QSize size, float pixelRatio, const QString& name, TestFunc testFunc)
{
    QImage referenceImage(size.width(), size.height(), QImage::Format_RGBA8888_Premultiplied);
    referenceImage.setDevicePixelRatio(pixelRatio);
    referenceImage.fill(Qt::white);

    RhiPaintDevice paintDevice;
    paintDevice.setSize(referenceImage.size());
    paintDevice.setDevicePixelRatio(referenceImage.devicePixelRatioF());

    testFunc(&paintDevice);
    testFunc(&referenceImage);

    RhiOffscreenRenderer offscreen(rhi.get(), referenceImage.size(), Qt::white);

    RhiPaintDeviceRenderer renderer(rhi.get());
    renderer.sync(&paintDevice);

    const auto rhiImage = offscreen.render(&renderer);

    const QString prefix = testDataDir() + "/" + "PaintEngineTest_" + name;

    rhiImage.save(prefix + "_rhi.png");
    referenceImage.save(prefix + "_qpainter.png");

    int accError = 0;
    imageDiff(rhiImage, referenceImage, accError).save(prefix + "_diff.png");

    const auto errPercent =
        100.0 * accError / (referenceImage.width() * referenceImage.height() * 255 * 3);
    std::cerr
        << name.toStdString()
        << " image error: " << errPercent << "%"
        << " or " << accError
        << "\n";

    if (rhi->backend() != QRhi::Null)
    {
        EXPECT_LE(errPercent, 0.5); //< This is more of a sanity check.
    }

    ASSERT_EQ(referenceImage.size(), rhiImage.size());
}

TEST_F(PaintEngineTest, textTransform)
{
    checkResult({400, 400}, 2, test_info_->name(),
        [](QPaintDevice* pd)
        {
            QPainter p(pd);

            p.rotate(15);
            auto f = p.font();
            f.setPixelSize(40);
            p.setFont(f);
            p.drawText(100, 100, "TEST");
        });
}

TEST_F(PaintEngineTest, linePatterns)
{
    checkResult({300, 300}, 2, test_info_->name(),
        [](QPaintDevice* pd)
        {
            QPainter p(pd);

            p.scale(2, 2);

            p.setPen(QPen(Qt::black, 0));
            p.drawLine(5, 0, 5, 50);

            QPen pen(Qt::black, 1, Qt::CustomDashLine);
            pen.setDashPattern({1, 2, 3});
            pen.setDashOffset(5);
            p.setPen(pen);
            p.drawLine(10, 0, 10, 50);

            pen.setStyle(Qt::DotLine);
            pen.setWidth(2);
            p.setPen(pen);
            p.drawLine(15, 0, 15, 50);

            p.setPen(QPen(Qt::black, 3, Qt::DashDotDotLine));
            p.drawLine(20, 0, 20, 50);

            QPen cp(Qt::black, 2, Qt::DashDotLine);
            cp.setCosmetic(true);
            p.setPen(cp);
            p.drawLine(25, 0, 25, 50);

            p.setPen(QPen(Qt::black, 4, Qt::DashLine));
            p.drawLine(30, 0, 30, 50);
        });
}

TEST_F(PaintEngineTest, opacityAndTransform)
{
    checkResult({300, 300}, 2, test_info_->name(),
        [](QPaintDevice* pd)
        {
            QPainter p(pd);

            p.setBrush(QColor(255, 0, 0, 100));

            p.drawRect(10, 10, 100, 100);

            p.save();
            p.translate(10, 10);
            p.rotate(-25);
            p.drawRect(50, 50, 50, 50);
            p.restore();
        });
}

TEST_F(PaintEngineTest, pathWithHole)
{
    checkResult({300, 300}, 2, test_info_->name(),
        [](QPaintDevice* pd)
        {
            QPainter p(pd);

            p.setBrush(Qt::blue);

            QRect hole(50, 50, 50, 50);

            QPainterPath fullPath;
            fullPath.addRect(hole);

            hole.adjust(10, 10, -10, -10);
            QPainterPath holePath;
            holePath.addRect(hole);

            p.setPen(QPen(Qt::black, 1));
            p.drawPath(fullPath.subtracted(holePath));
        });
}

TEST_F(PaintEngineTest, polygon)
{
    checkResult({400, 400}, 2, test_info_->name(),
        [](QPaintDevice* pd)
        {
            QPainter p(pd);

            QPolygon poly;
            QList<QPoint> points;
            points << QPoint(100, 100);
            points << QPoint(130, 100);
            points << QPoint(140, 140);
            points << QPoint(100, 130);
            poly.append(points);
            p.drawPolygon(poly);
        });
}

TEST_F(PaintEngineTest, pixmapClipping)
{
    checkResult({400, 400}, 2, test_info_->name(),
        [](QPaintDevice* pd)
        {
            QPainter p(pd);

            // Currently RHI always does smooth pixmap transform.
            p.setRenderHint(QPainter::SmoothPixmapTransform);

            QImage textImage(100, 100, QImage::Format_RGBA8888_Premultiplied);
            textImage.setDevicePixelRatio(2);
            textImage.fill(Qt::white);
            {
                QPainter pp(&textImage);
                pp.setPen(Qt::black);
                pp.setBrush(Qt::black);
                pp.drawLine(0, 0, 50, 50);
                auto f = p.font();
                f.setPixelSize(20);
                pp.setFont(f);
                pp.drawText(0, 20, "TEXT");
            }
            // It is important to transform the image into pixmap ourselves otherwise Qt does it
            // for us but resulting internal pixmap would be extracted from source rect.
            const QPixmap textPixmap = QPixmap::fromImage(textImage);

            p.drawPixmap(QRect(20, 20, 50, 50), textPixmap);

            p.setClipRect(100, 82, 40, 40);
            p.drawPixmap(QRect(100, 82, 50, 50), textPixmap, QRectF(10, 10, 30, 80));
        });
}

TEST_F(PaintEngineTest, imageClipAndTransform)
{
    checkResult({400, 400}, 2, test_info_->name(),
        [](QPaintDevice* pd)
        {
            QPainter p(pd);

            // Currently RHI always does smooth pixmap transform.
            p.setRenderHint(QPainter::SmoothPixmapTransform);

            p.setPen(Qt::black);
            p.setBrush(QColor(255, 0, 0, 100));

            // Prepare a pixmap that would be used throughout the test.
            QImage pix(50, 50, QImage::Format_RGBA8888_Premultiplied);
            pix.setDevicePixelRatio(2);
            pix.fill(Qt::black);
            QPainter tmp(&pix);
            tmp.fillRect(10, 10, 30, 30, Qt::green);

            // Test clipping with transform.
            p.save();
            p.setClipRect(QRect(0, 0, 200, 160));
            p.rotate(15);
            p.setOpacity(0.5);
            p.setClipRect(QRect(70, 125, 40, 25), Qt::ClipOperation::IntersectClip);
            p.drawImage(QRect(50, 100, 100, 50), pix, QRect(5, 5, 40, 40));
            p.restore();

            // Test image drawing.
            p.drawImage(10, 170, pix);

            p.drawImage(QRect(80, 130, 100, 50), pix, QRect(5, 5, 40, 40));
            p.rotate(20);
            p.drawImage(QRect(80, 30, 100, 50), pix, QRect(5, 5, 40, 40));
        });
}

} // namespace test

} // namespace namespace nx::pathkit
