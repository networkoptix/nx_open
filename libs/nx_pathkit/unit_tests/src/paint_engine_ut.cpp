// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#if QT_VERSION >= QT_VERSION_CHECK(6,6,0)
    #include <rhi/qrhi.h>
#else
    #include <QtGui/private/qrhi_p.h>
    #if defined(Q_OS_MACOS)
        #include <QtGui/private/qrhimetal_p.h>
    #elif defined(Q_OS_WIN)
        #include <QtGui/private/qrhid3d11_p.h>
    #endif
    #include <QtGui/private/qrhigles2_p.h>
    #include <QtGui/QOffscreenSurface>
#endif

#include <QtGui/QPainterPath>

#include <nx/pathkit/rhi_paint_device.h>
#include <nx/pathkit/rhi_paint_engine.h>

#include "rhi_offscreen_renderer.h"

namespace nx::pathkit {

namespace test {

class PaintEngineTest: public ::testing::Test
{
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

TEST_F(PaintEngineTest, checkRendering)
{
    QImage referenceImage(400, 400, QImage::Format_RGBA8888_Premultiplied);
    referenceImage.setDevicePixelRatio(2);
    referenceImage.fill(Qt::white);

    RhiPaintDevice paintDevice;
    paintDevice.setSize(referenceImage.size());
    paintDevice.setDevicePixelRatio(referenceImage.devicePixelRatioF());

    const auto paint =
        [](QPaintDevice* pd)
        {
            QPainter p(pd);
            p.setPen(Qt::black);
            p.setBrush(QColor(255, 0, 0, 100));

            QImage pix(50, 50, QImage::Format_RGBA8888_Premultiplied);
            pix.setDevicePixelRatio(2);
            pix.fill(Qt::black);
            QPainter tmp(&pix);
            tmp.fillRect(10, 10, 30, 30, Qt::green);

            p.drawRect(0, 0, 100, 100);

            p.save();
            p.translate(10, 10);
            p.rotate(-25);
            p.drawRect(50, 50, 50, 50);
            p.restore();

            p.save();
            p.rotate(15);
            auto f = p.font();
            f.setPixelSize(20);
            p.setFont(f);
            p.drawText(100, 100, "TEST");
            p.restore();

            p.save();
            p.rotate(15);
            p.setOpacity(0.5);
            p.drawImage(QRect(50, 100, 100, 50), pix, QRect(5, 5, 40, 40));
            p.restore();

            p.drawImage(QRect(80, 130, 100, 50), pix, QRect(5, 5, 40, 40));
            p.drawImage(QRect(80, 30, 100, 50), pix, QRect(5, 5, 40, 40));

            auto pix2 = QPixmap::fromImage(pix);
            p.drawPixmap(200, 200, pix2);
            p.drawPixmap(220, 220, pix2);

            p.setPen(QPen(Qt::black, 2));

            QPolygon poly;
            QList<QPoint> points;
            points << QPoint(100, 100);
            points << QPoint(130, 100);
            points << QPoint(140, 140);
            points << QPoint(100, 130);
            poly.append(points);
            p.drawPolygon(poly);

            p.save();
            p.scale(2, 2);

            p.setPen(QPen(Qt::black, 0));
            p.drawLine(5, 0, 5, 50);

            p.setPen(QPen(Qt::black, 1));
            p.drawLine(10, 0, 10, 50);

            p.setPen(QPen(Qt::black, 2));
            p.drawLine(15, 0, 15, 50);

            p.setPen(QPen(Qt::black, 3));
            p.drawLine(20, 0, 20, 50);

            QPen cp(Qt::black, 2);
            cp.setCosmetic(true);
            p.setPen(cp);
            p.setPen(QPen(Qt::black, 3));
            p.drawLine(25, 0, 25, 50);
            p.restore();

            p.setBrush(Qt::blue);

            QRect hole(140, 140, 50, 50);

            QPainterPath fullPath;
            fullPath.addRect(hole);

            hole.adjust(10, 10, -10, -10);
            QPainterPath holePath;
            holePath.addRect(hole);

            p.setPen(QPen(Qt::black, 1));
            p.drawPath(fullPath.subtracted(holePath));
        };

    paint(&paintDevice);
    paint(&referenceImage);

    RhiOffscreenRenderer offscreen(rhi.get(), referenceImage.size(), Qt::white);

    RhiPaintDeviceRenderer renderer(rhi.get());
    renderer.sync(&paintDevice);

    const auto rhiImage = offscreen.render(&renderer);

    rhiImage.save("frame_rhi.png");
    referenceImage.save("frame_qpainter.png");

    int accError = 0;
    imageDiff(rhiImage, referenceImage, accError).save("frame_diff.png");

    const auto errPercent =
        100.0 * accError / (referenceImage.width() * referenceImage.height() * 255 * 3);
    std::cerr << "Image error: " << accError << " or " << errPercent << "%" <<"\n";

    if (rhi->backend() != QRhi::Null)
    {
        EXPECT_LE(errPercent, 0.5); //< This is more of a sanity check.
    }

    ASSERT_EQ(referenceImage.size(), rhiImage.size());
}

} // namespace test

} // namespace namespace nx::pathkit
