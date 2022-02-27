// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <memory>

#include <transcoding/filters/dewarping_image_filter.h>
#include <utils/common/aspect_ratio.h>

std::ostream& operator<<(std::ostream& s, const QPoint& point)
{
    return s << "{x: " << point.x() << ", y: " << point.y() << "}";
}

std::ostream& operator<<(std::ostream& s, const QPointF& point)
{
    return s << "{x: " << point.x() << ", y: " << point.y() << "}";
}

std::ostream& operator<<(std::ostream& s, const QSize& size)
{
    return s << "{width: " << size.width() << ", height: " << size.height() << "}";
}

namespace transcoding::filters::test {

using namespace nx::vms::api::dewarping;

struct When
{
    const CameraProjection cameraProjection;
    const FisheyeCameraMount mount = FisheyeCameraMount::wall;
    const int panoFactor = 1;
    const QPoint sourceCoords;
    const qreal cameraRollDegrees = 0;
    const QPointF horizonCorrectionDegrees{0, 0};
    const QPointF viewAngleDegrees{0, 0};
    const qreal fovDegrees = 30;
    const QSize frameSize;

    When( //< Fisheye constructor.
        const CameraProjection cameraProjection,
        const FisheyeCameraMount mount,
        const int panoFactor,
        const QPoint& sourceCoords,
        const qreal cameraRollDegrees = 0,
        const QPointF& viewAngleDegrees = QPointF(),
        const qreal fovDegrees = 30,
        const QSize& frameSize = QSize(1920, 1080))
        :
        cameraProjection(cameraProjection),
        mount(mount),
        panoFactor(panoFactor),
        sourceCoords(sourceCoords),
        cameraRollDegrees(cameraRollDegrees),
        viewAngleDegrees(viewAngleDegrees),
        fovDegrees(fovDegrees),
        frameSize(frameSize)
    {
    }

    When( //< 360VR constructor.
        const int panoFactor,
        const QPoint& sourceCoords,
        const QPointF& horizonCorrectionDegrees = QPointF(),
        const QPointF& viewAngleDegrees = QPointF(),
        const qreal fovDegrees = 30,
        const QSize& frameSize = QSize(2048, 1024))
        :
        cameraProjection(CameraProjection::equirectangular360),
        panoFactor(panoFactor),
        sourceCoords(sourceCoords),
        horizonCorrectionDegrees(horizonCorrectionDegrees),
        viewAngleDegrees(viewAngleDegrees),
        fovDegrees(fovDegrees),
        frameSize(frameSize)
    {
    }
};

struct Expect
{
    const QPoint targetCoords;
    const QSize targetFrameSize{1920, 1080};

    Expect(const QPoint& targetCoords, const QSize& targetFrameSize = QSize(1920, 1080)):
        targetCoords(targetCoords),
        targetFrameSize(targetFrameSize)
    {
    }
};

struct TestParams
{
    When when;
    Expect expect;
};

void PrintTo(const TestParams& params, ::std::ostream* s)
{
    *s << "\n\t";
    if (MediaData::isFisheye(params.when.cameraProjection))
    {
        *s << "Fisheye lens projection: " << params.when.cameraProjection << "\n\t";
        *s << "Mount: " << params.when.mount << "\n\t";
        *s << "Pano factor: " << params.when.panoFactor << "\n\t";
        *s << "Camera roll (degrees): " << params.when.cameraRollDegrees << "\n\t";
    }
    else
    {
        *s << "360VR camera projection: " << params.when.cameraProjection << "\n\t";
        *s << "Pano factor: " << params.when.panoFactor << "\n\t";
        *s << "Horizon correction (degrees): " << params.when.horizonCorrectionDegrees << "\n\t";
    }

    *s << "View rotation (degrees): " << params.when.viewAngleDegrees << "\n\t";
    *s << "FOV (degrees): " << params.when.fovDegrees << "\n\t";
    *s << "Frame size: " << params.when.frameSize << "\n\t";
    *s << "Source coords: " << params.when.sourceCoords << "\n";
}

class DewarpingImageFilterTest:
    public testing::TestWithParam<TestParams>,
    public QnDewarpingImageFilter
{
protected:
    void setParameters(const When& when)
    {
        MediaData mediaData;
        mediaData.enabled = true;
        mediaData.cameraProjection = when.cameraProjection;

        if (MediaData::isFisheye(when.cameraProjection))
        {
            mediaData.viewMode = when.mount;
            mediaData.fovRot = when.cameraRollDegrees;

            // For 1920x1080 frame these parameters give 1800x1000 fisheye ellipse with center
            // at {1000; 1000} - i.e. an ellipse inscribed into {100; 0} - {1900; 1000} rectangle.
            mediaData.xCenter = 0.5208;
            mediaData.yCenter = 0.4630;
            mediaData.radius = 0.4688;
            mediaData.hStretch = 1.8;
        }
        else
        {
            mediaData.sphereAlpha = when.horizonCorrectionDegrees.x();
            mediaData.sphereBeta = when.horizonCorrectionDegrees.y();
        }

        ViewData viewData;
        viewData.enabled = true;
        viewData.xAngle = qDegreesToRadians(when.viewAngleDegrees.x());
        viewData.yAngle = qDegreesToRadians(when.viewAngleDegrees.y());
        viewData.fov = qDegreesToRadians(when.fovDegrees);
        viewData.panoFactor = when.panoFactor;

        QnDewarpingImageFilter::setParameters(mediaData, viewData);
        m_viewSize = updatedResolution(when.frameSize);
        updateDewarpingParameters(m_viewSize, QnAspectRatio(when.frameSize).toFloat());
    }

    QPoint transformed(const QPoint& viewCoord) const
    {
        return QnDewarpingImageFilter::transformed(
            viewCoord.x(), viewCoord.y(), m_viewSize).toPoint();
    }

protected:
    QSize m_viewSize;
};

TEST_P(DewarpingImageFilterTest, Transformations)
{
    TestParams params = GetParam();
    setParameters(params.when);
    ASSERT_EQ(m_viewSize, params.expect.targetFrameSize);
    ASSERT_EQ(transformed(params.when.sourceCoords), params.expect.targetCoords);
}

static const std::vector<TestParams> kDewarpingImageFilterTestParams({
    // Rectilinear view, Equidistant fisheye lens, Wall mount.
    TestParams{
        When(
            CameraProjection::equidistant,
            FisheyeCameraMount::wall,
            /*panoFactor*/1,
            /*sourceCoords*/QPoint(960, 540),
            /*cameraRollDegrees*/45),
        Expect(
            /*targetCoords*/QPoint(1000, 500))},
    TestParams{
        When(
            CameraProjection::equidistant,
            FisheyeCameraMount::wall,
            /*panoFactor*/1,
            /*sourceCoords*/QPoint(480, 810),
            /*cameraRollDegrees*/45),
        Expect(
            /*targetCoords*/QPoint(916, 487))},
    TestParams{
        When(
            CameraProjection::equidistant,
            FisheyeCameraMount::wall,
            /*panoFactor*/1,
            /*sourceCoords*/QPoint(960, 540),
            /*cameraRollDegrees*/45,
            /*viewAngleDegrees*/QPointF(-40, 30)),
        Expect(
            /*targetCoords*/QPoint(974, 231))},
    TestParams{
        When(
            CameraProjection::equidistant,
            FisheyeCameraMount::wall,
            /*panoFactor*/1,
            /*sourceCoords*/QPoint(480, 810),
            /*cameraRollDegrees*/45,
            /*viewAngleDegrees*/QPointF(-40, 30)),
        Expect(
            /*targetCoords*/QPoint(884, 212))},

    // Rectilinear view, Stereographic fisheye lens, Wall mount.
    TestParams{
        When(
            CameraProjection::stereographic,
            FisheyeCameraMount::wall,
            /*panoFactor*/1,
            /*sourceCoords*/QPoint(960, 540),
            /*cameraRollDegrees*/45),
        Expect(
            /*targetCoords*/QPoint(1000, 500))},
    TestParams{
        When(
            CameraProjection::stereographic,
            FisheyeCameraMount::wall,
            /*panoFactor*/1,
            /*sourceCoords*/QPoint(480, 810),
            /*cameraRollDegrees*/45),
        Expect(
            /*targetCoords*/QPoint(934, 490))},
    TestParams{
        When(
            CameraProjection::stereographic,
            FisheyeCameraMount::wall,
            /*panoFactor*/1,
            /*sourceCoords*/QPoint(960, 540),
            /*cameraRollDegrees*/45,
            /*viewAngleDegrees*/QPointF(-40, 30)),
        Expect(
            /*targetCoords*/QPoint(978, 275))},
    TestParams{
        When(
            CameraProjection::stereographic,
            FisheyeCameraMount::wall,
            /*panoFactor*/1,
            /*sourceCoords*/QPoint(480, 810),
            /*cameraRollDegrees*/45,
            /*viewAngleDegrees*/QPointF(-40, 30)),
        Expect(
            /*targetCoords*/QPoint(902, 256))},

    // Rectilinear view, Equisolid fisheye lens, Wall mount.
    TestParams{
        When(
            CameraProjection::equisolid,
            FisheyeCameraMount::wall,
            /*panoFactor*/1,
            /*sourceCoords*/QPoint(960, 540),
            /*cameraRollDegrees*/45),
        Expect(
            /*targetCoords*/QPoint(1000, 500))},
    TestParams{
        When(
            CameraProjection::equisolid,
            FisheyeCameraMount::wall,
            /*panoFactor*/1,
            /*sourceCoords*/QPoint(480, 810),
            /*cameraRollDegrees*/45),
        Expect(
            /*targetCoords*/QPoint(907, 486))},
    TestParams{
        When(
            CameraProjection::equisolid,
            FisheyeCameraMount::wall,
            /*panoFactor*/1,
            /*sourceCoords*/QPoint(960, 540),
            /*cameraRollDegrees*/45,
            /*viewAngleDegrees*/QPointF(-40, 30)),
        Expect(
            /*targetCoords*/QPoint(972, 210))},
    TestParams{
        When(
            CameraProjection::equisolid,
            FisheyeCameraMount::wall,
            /*panoFactor*/1,
            /*sourceCoords*/QPoint(480, 810),
            /*cameraRollDegrees*/45,
            /*viewAngleDegrees*/QPointF(-40, 30)),
        Expect(
            /*targetCoords*/QPoint(876, 191))},

    // Equirectangular view (180 degrees), Equidistant fisheye lens, Wall mount.
    TestParams{
        When(
            CameraProjection::equidistant,
            FisheyeCameraMount::wall,
            /*panoFactor*/2,
            /*sourceCoords*/QPoint(1016, 509),
            /*cameraRollDegrees*/45),
        Expect(
            /*targetCoords*/QPoint(1058, 471),
            /*targetFrameSize*/QSize(2032, 1018))},
    TestParams{
        When(
            CameraProjection::equidistant,
            FisheyeCameraMount::wall,
            /*panoFactor*/2,
            /*sourceCoords*/QPoint(508, 764),
            /*cameraRollDegrees*/45),
        Expect(
            /*targetCoords*/QPoint(974, 458),
            /*targetFrameSize*/QSize(2032, 1018))},
    TestParams{
        When(
            CameraProjection::equidistant,
            FisheyeCameraMount::wall,
            /*panoFactor*/2,
            /*sourceCoords*/QPoint(1058, 471),
            /*cameraRollDegrees*/45,
            /*viewAngleDegrees*/QPointF(-40, 30)),
        Expect(
            /*targetCoords*/QPoint(1040, 219),
            /*targetFrameSize*/QSize(2032, 1018))},
    TestParams{
        When(
            CameraProjection::equidistant,
            FisheyeCameraMount::wall,
            /*panoFactor*/2,
            /*sourceCoords*/QPoint(508, 764),
            /*cameraRollDegrees*/45,
            /*viewAngleDegrees*/QPointF(-40, 30)),
        Expect(
            /*targetCoords*/QPoint(950, 201),
            /*targetFrameSize*/QSize(2032, 1018))},

    // Equirectangular view (180 degrees), Stereographic fisheye lens, Wall mount.
    TestParams{
        When(
            CameraProjection::stereographic,
            FisheyeCameraMount::wall,
            /*panoFactor*/2,
            /*sourceCoords*/QPoint(1016, 509),
            /*cameraRollDegrees*/45),
        Expect(
            /*targetCoords*/QPoint(1058, 471),
            /*targetFrameSize*/QSize(2032, 1018))},
    TestParams{
        When(
            CameraProjection::stereographic,
            FisheyeCameraMount::wall,
            /*panoFactor*/2,
            /*sourceCoords*/QPoint(508, 764),
            /*cameraRollDegrees*/45),
        Expect(
            /*targetCoords*/QPoint(992, 460),
            /*targetFrameSize*/QSize(2032, 1018))},
    TestParams{
        When(
            CameraProjection::stereographic,
            FisheyeCameraMount::wall,
            /*panoFactor*/2,
            /*sourceCoords*/QPoint(1058, 471),
            /*cameraRollDegrees*/45,
            /*viewAngleDegrees*/QPointF(-40, 30)),
        Expect(
            /*targetCoords*/QPoint(1043, 260),
            /*targetFrameSize*/QSize(2032, 1018))},
    TestParams{
        When(
            CameraProjection::stereographic,
            FisheyeCameraMount::wall,
            /*panoFactor*/2,
            /*sourceCoords*/QPoint(508, 764),
            /*cameraRollDegrees*/45,
            /*viewAngleDegrees*/QPointF(-40, 30)),
        Expect(
            /*targetCoords*/QPoint(966, 242),
            /*targetFrameSize*/QSize(2032, 1018))},

    // Equirectangular view (180 degrees), Equisolid fisheye lens, Wall mount.
    TestParams{
        When(
            CameraProjection::equisolid,
            FisheyeCameraMount::wall,
            /*panoFactor*/2,
            /*sourceCoords*/QPoint(1016, 509),
            /*cameraRollDegrees*/45),
        Expect(
            /*targetCoords*/QPoint(1058, 471),
            /*targetFrameSize*/QSize(2032, 1018))},
    TestParams{
        When(
            CameraProjection::equisolid,
            FisheyeCameraMount::wall,
            /*panoFactor*/2,
            /*sourceCoords*/QPoint(508, 764),
            /*cameraRollDegrees*/45),
        Expect(
            /*targetCoords*/QPoint(965, 456),
            /*targetFrameSize*/QSize(2032, 1018))},
    TestParams{
        When(
            CameraProjection::equisolid,
            FisheyeCameraMount::wall,
            /*panoFactor*/2,
            /*sourceCoords*/QPoint(1058, 471),
            /*cameraRollDegrees*/45,
            /*viewAngleDegrees*/QPointF(-40, 30)),
        Expect(
            /*targetCoords*/QPoint(1038, 199),
            /*targetFrameSize*/QSize(2032, 1018))},
    TestParams{
        When(
            CameraProjection::equisolid,
            FisheyeCameraMount::wall,
            /*panoFactor*/2,
            /*sourceCoords*/QPoint(508, 764),
            /*cameraRollDegrees*/45,
            /*viewAngleDegrees*/QPointF(-40, 30)),
        Expect(
            /*targetCoords*/QPoint(942, 181),
            /*targetFrameSize*/QSize(2032, 1018))},

    // Equirectangular view (360 degrees), Equidistant fisheye lens, Wall mount.
    TestParams{
        When(
            CameraProjection::equidistant,
            FisheyeCameraMount::wall,
            /*panoFactor*/4,
            /*sourceCoords*/QPoint(1440, 360),
            /*cameraRollDegrees*/45),
        Expect(
            /*targetCoords*/QPoint(1500, 333),
            /*targetFrameSize*/QSize(2880, 720))},
    TestParams{
        When(
            CameraProjection::equidistant,
            FisheyeCameraMount::wall,
            /*panoFactor*/4,
            /*sourceCoords*/QPoint(720, 540),
            /*cameraRollDegrees*/45),
        Expect(
            /*targetCoords*/QPoint(1400, 319),
            /*targetFrameSize*/QSize(2880, 720))},
    TestParams{
        When(
            CameraProjection::equidistant,
            FisheyeCameraMount::wall,
            /*panoFactor*/4,
            /*sourceCoords*/QPoint(1440, 360),
            /*cameraRollDegrees*/45,
            /*viewAngleDegrees*/QPointF(-40, 30)),
        Expect(
            /*targetCoords*/QPoint(1461, 154),
            /*targetFrameSize*/QSize(2880, 720))},
    TestParams{
        When(
            CameraProjection::equidistant,
            FisheyeCameraMount::wall,
            /*panoFactor*/4,
            /*sourceCoords*/QPoint(720, 540),
            /*cameraRollDegrees*/45,
            /*viewAngleDegrees*/QPointF(-40, 30)),
        Expect(
            /*targetCoords*/QPoint(1374, 138),
            /*targetFrameSize*/QSize(2880, 720))},

    // Equirectangular view (360 degrees), Stereographic fisheye lens, Wall mount.
    TestParams{
        When(
            CameraProjection::stereographic,
            FisheyeCameraMount::wall,
            /*panoFactor*/4,
            /*sourceCoords*/QPoint(1440, 360),
            /*cameraRollDegrees*/45),
        Expect(
            /*targetCoords*/QPoint(1500, 333),
            /*targetFrameSize*/QSize(2880, 720))},
    TestParams{
        When(
            CameraProjection::stereographic,
            FisheyeCameraMount::wall,
            /*panoFactor*/4,
            /*sourceCoords*/QPoint(720, 540),
            /*cameraRollDegrees*/45),
        Expect(
            /*targetCoords*/QPoint(1422, 322),
            /*targetFrameSize*/QSize(2880, 720))},
    TestParams{
        When(
            CameraProjection::stereographic,
            FisheyeCameraMount::wall,
            /*panoFactor*/4,
            /*sourceCoords*/QPoint(1440, 360),
            /*cameraRollDegrees*/45,
            /*viewAngleDegrees*/QPointF(-40, 30)),
        Expect(
            /*targetCoords*/QPoint(1467, 184),
            /*targetFrameSize*/QSize(2880, 720))},
    TestParams{
        When(
            CameraProjection::stereographic,
            FisheyeCameraMount::wall,
            /*panoFactor*/4,
            /*sourceCoords*/QPoint(720, 540),
            /*cameraRollDegrees*/45,
            /*viewAngleDegrees*/QPointF(-40, 30)),
        Expect(
            /*targetCoords*/QPoint(1393, 168),
            /*targetFrameSize*/QSize(2880, 720))},

    // Equirectangular view (360 degrees), Equisolid fisheye lens, Wall mount.
    TestParams{
        When(
            CameraProjection::equisolid,
            FisheyeCameraMount::wall,
            /*panoFactor*/4,
            /*sourceCoords*/QPoint(1440, 360),
            /*cameraRollDegrees*/45),
        Expect(
            /*targetCoords*/QPoint(1500, 333),
            /*targetFrameSize*/QSize(2880, 720))},
    TestParams{
        When(
            CameraProjection::equisolid,
            FisheyeCameraMount::wall,
            /*panoFactor*/4,
            /*sourceCoords*/QPoint(720, 540),
            /*cameraRollDegrees*/45),
        Expect(
            /*targetCoords*/QPoint(1389, 317),
            /*targetFrameSize*/QSize(2880, 720))},
    TestParams{
        When(
            CameraProjection::equisolid,
            FisheyeCameraMount::wall,
            /*panoFactor*/4,
            /*sourceCoords*/QPoint(1440, 360),
            /*cameraRollDegrees*/45,
            /*viewAngleDegrees*/QPointF(-40, 30)),
        Expect(
            /*targetCoords*/QPoint(1458, 140),
            /*targetFrameSize*/QSize(2880, 720))},
    TestParams{
        When(
            CameraProjection::equisolid,
            FisheyeCameraMount::wall,
            /*panoFactor*/4,
            /*sourceCoords*/QPoint(720, 540),
            /*cameraRollDegrees*/45,
            /*viewAngleDegrees*/QPointF(-40, 30)),
        Expect(
            /*targetCoords*/QPoint(1365, 124),
            /*targetFrameSize*/QSize(2880, 720))},

    // Rectilinear view, Equidistant fisheye lens, Table mount.
    TestParams{
        When(
            CameraProjection::equidistant,
            FisheyeCameraMount::table,
            /*panoFactor*/1,
            /*sourceCoords*/QPoint(960, 540),
            /*cameraRollDegrees*/0,
            /*viewAngleDegrees*/QPointF(0, 15)),
        Expect(
            /*targetCoords*/QPoint(1000, 84))},
    TestParams{
        When(
            CameraProjection::equidistant,
            FisheyeCameraMount::table,
            /*panoFactor*/1,
            /*sourceCoords*/QPoint(480, 810),
            /*cameraRollDegrees*/0,
            /*viewAngleDegrees*/QPointF(0, 15)),
        Expect(
            /*targetCoords*/QPoint(1107, 64))},
    TestParams{
        When(
            CameraProjection::equidistant,
            FisheyeCameraMount::table,
            /*panoFactor*/1,
            /*sourceCoords*/QPoint(960, 540),
            /*cameraRollDegrees*/45,
            /*viewAngleDegrees*/QPointF(120, 30)),
        Expect(
            /*targetCoords*/QPoint(422, 414))},
    TestParams{
        When(
            CameraProjection::equidistant,
            FisheyeCameraMount::table,
            /*panoFactor*/1,
            /*sourceCoords*/QPoint(480, 810),
            /*cameraRollDegrees*/45,
            /*viewAngleDegrees*/QPointF(120, 30)),
        Expect(
            /*targetCoords*/QPoint(409, 358))},

    // Rectilinear view, Stereographic fisheye lens, Table mount.
    TestParams{
        When(
            CameraProjection::stereographic,
            FisheyeCameraMount::table,
            /*panoFactor*/1,
            /*sourceCoords*/QPoint(960, 540),
            /*cameraRollDegrees*/0,
            /*viewAngleDegrees*/QPointF(0, 15)),
        Expect(
            /*targetCoords*/QPoint(1000, 117))},
    TestParams{
        When(
            CameraProjection::stereographic,
            FisheyeCameraMount::table,
            /*panoFactor*/1,
            /*sourceCoords*/QPoint(480, 810),
            /*cameraRollDegrees*/0,
            /*viewAngleDegrees*/QPointF(0, 15)),
        Expect(
            /*targetCoords*/QPoint(1100, 90))},
    TestParams{
        When(
            CameraProjection::stereographic,
            FisheyeCameraMount::table,
            /*panoFactor*/1,
            /*sourceCoords*/QPoint(960, 540),
            /*cameraRollDegrees*/45,
            /*viewAngleDegrees*/QPointF(120, 30)),
        Expect(
            /*targetCoords*/QPoint(499, 426))},
    TestParams{
        When(
            CameraProjection::stereographic,
            FisheyeCameraMount::table,
            /*panoFactor*/1,
            /*sourceCoords*/QPoint(480, 810),
            /*cameraRollDegrees*/45,
            /*viewAngleDegrees*/QPointF(120, 30)),
        Expect(
            /*targetCoords*/QPoint(480, 375))},

    // Rectilinear view, Equisolid fisheye lens, Table mount.
    TestParams{
        When(
            CameraProjection::equisolid,
            FisheyeCameraMount::table,
            /*panoFactor*/1,
            /*sourceCoords*/QPoint(960, 540),
            /*cameraRollDegrees*/0,
            /*viewAngleDegrees*/QPointF(0, 15)),
        Expect(
            /*targetCoords*/QPoint(1000, 70))},
    TestParams{
        When(
            CameraProjection::equisolid,
            FisheyeCameraMount::table,
            /*panoFactor*/1,
            /*sourceCoords*/QPoint(480, 810),
            /*cameraRollDegrees*/0,
            /*viewAngleDegrees*/QPointF(0, 15)),
        Expect(
            /*targetCoords*/QPoint(1109, 53))},
    TestParams{
        When(
            CameraProjection::equisolid,
            FisheyeCameraMount::table,
            /*panoFactor*/1,
            /*sourceCoords*/QPoint(960, 540),
            /*cameraRollDegrees*/45,
            /*viewAngleDegrees*/QPointF(120, 30)),
        Expect(
            /*targetCoords*/QPoint(386, 409))},
    TestParams{
        When(
            CameraProjection::equisolid,
            FisheyeCameraMount::table,
            /*panoFactor*/1,
            /*sourceCoords*/QPoint(480, 810),
            /*cameraRollDegrees*/45,
            /*viewAngleDegrees*/QPointF(120, 30)),
        Expect(
            /*targetCoords*/QPoint(378, 350))},

    // Equirectangular view (180 degrees), Equidistant fisheye lens, Table mount.
    TestParams{
        When(
            CameraProjection::equidistant,
            FisheyeCameraMount::table,
            /*panoFactor*/2,
            /*sourceCoords*/QPoint(1016, 509),
            /*cameraRollDegrees*/0,
            /*viewAngleDegrees*/QPointF(0, 15)),
        Expect(
            /*targetCoords*/QPoint(1058, 79),
            /*targetFrameSize*/QSize(2032, 1018))},
    TestParams{
        When(
            CameraProjection::equidistant,
            FisheyeCameraMount::table,
            /*panoFactor*/2,
            /*sourceCoords*/QPoint(508, 764),
            /*cameraRollDegrees*/0,
            /*viewAngleDegrees*/QPointF(0, 15)),
        Expect(
            /*targetCoords*/QPoint(1167, 62),
            /*targetFrameSize*/QSize(2032, 1018))},
    TestParams{
        When(
            CameraProjection::equidistant,
            FisheyeCameraMount::table,
            /*panoFactor*/2,
            /*sourceCoords*/QPoint(1016, 509),
            /*cameraRollDegrees*/45,
            /*viewAngleDegrees*/QPointF(120, 30)),
        Expect(
            /*targetCoords*/QPoint(445, 390),
            /*targetFrameSize*/QSize(2032, 1018))},
    TestParams{
        When(
            CameraProjection::equidistant,
            FisheyeCameraMount::table,
            /*panoFactor*/2,
            /*sourceCoords*/QPoint(508, 764),
            /*cameraRollDegrees*/45,
            /*viewAngleDegrees*/QPointF(120, 30)),
        Expect(
            /*targetCoords*/QPoint(435, 344),
            /*targetFrameSize*/QSize(2032, 1018))},

    // Equirectangular view (180 degrees), Stereographic fisheye lens, Table mount.
    TestParams{
        When(
            CameraProjection::stereographic,
            FisheyeCameraMount::table,
            /*panoFactor*/2,
            /*sourceCoords*/QPoint(1016, 509),
            /*cameraRollDegrees*/0,
            /*viewAngleDegrees*/QPointF(0, 15)),
        Expect(
            /*targetCoords*/QPoint(1058, 110),
            /*targetFrameSize*/QSize(2032, 1018))},
    TestParams{
        When(
            CameraProjection::stereographic,
            FisheyeCameraMount::table,
            /*panoFactor*/2,
            /*sourceCoords*/QPoint(508, 764),
            /*cameraRollDegrees*/0,
            /*viewAngleDegrees*/QPointF(0, 15)),
        Expect(
            /*targetCoords*/QPoint(1160, 88),
            /*targetFrameSize*/QSize(2032, 1018))},
    TestParams{
        When(
            CameraProjection::stereographic,
            FisheyeCameraMount::table,
            /*panoFactor*/2,
            /*sourceCoords*/QPoint(1016, 509),
            /*cameraRollDegrees*/45,
            /*viewAngleDegrees*/QPointF(120, 30)),
        Expect(
            /*targetCoords*/QPoint(527, 401),
            /*targetFrameSize*/QSize(2032, 1018))},
    TestParams{
        When(
            CameraProjection::stereographic,
            FisheyeCameraMount::table,
            /*panoFactor*/2,
            /*sourceCoords*/QPoint(508, 764),
            /*cameraRollDegrees*/45,
            /*viewAngleDegrees*/QPointF(120, 30)),
        Expect(
            /*targetCoords*/QPoint(511, 359),
            /*targetFrameSize*/QSize(2032, 1018))},

    // Equirectangular view (180 degrees), Equisolid fisheye lens, Table mount.
    TestParams{
        When(
            CameraProjection::equisolid,
            FisheyeCameraMount::table,
            /*panoFactor*/2,
            /*sourceCoords*/QPoint(1016, 509),
            /*cameraRollDegrees*/0,
            /*viewAngleDegrees*/QPointF(0, 15)),
        Expect(
            /*targetCoords*/QPoint(1058, 66),
            /*targetFrameSize*/QSize(2032, 1018))},
    TestParams{
        When(
            CameraProjection::equisolid,
            FisheyeCameraMount::table,
            /*panoFactor*/2,
            /*sourceCoords*/QPoint(508, 764),
            /*cameraRollDegrees*/0,
            /*viewAngleDegrees*/QPointF(0, 15)),
        Expect(
            /*targetCoords*/QPoint(1170, 52),
            /*targetFrameSize*/QSize(2032, 1018))},
    TestParams{
        When(
            CameraProjection::equisolid,
            FisheyeCameraMount::table,
            /*panoFactor*/2,
            /*sourceCoords*/QPoint(1016, 509),
            /*cameraRollDegrees*/45,
            /*viewAngleDegrees*/QPointF(120, 30)),
        Expect(
            /*targetCoords*/QPoint(408, 385),
            /*targetFrameSize*/QSize(2032, 1018))},
    TestParams{
        When(
            CameraProjection::equisolid,
            FisheyeCameraMount::table,
            /*panoFactor*/2,
            /*sourceCoords*/QPoint(508, 764),
            /*cameraRollDegrees*/45,
            /*viewAngleDegrees*/QPointF(120, 30)),
        Expect(
            /*targetCoords*/QPoint(401, 337),
            /*targetFrameSize*/QSize(2032, 1018))},

    // Equirectangular view (360 degrees), Equidistant fisheye lens, Table mount.
    TestParams{
        When(
            CameraProjection::equidistant,
            FisheyeCameraMount::table,
            /*panoFactor*/4,
            /*sourceCoords*/QPoint(1440, 360),
            /*cameraRollDegrees*/0,
            /*viewAngleDegrees*/QPointF(0, 15)),
        Expect(
            /*targetCoords*/QPoint(1500, 56),
            /*targetFrameSize*/QSize(2880, 720))},
    TestParams{
        When(
            CameraProjection::equidistant,
            FisheyeCameraMount::table,
            /*panoFactor*/4,
            /*sourceCoords*/QPoint(720, 540),
            /*cameraRollDegrees*/0,
            /*viewAngleDegrees*/QPointF(0, 15)),
        Expect(
            /*targetCoords*/QPoint(1650, 51),
            /*targetFrameSize*/QSize(2880, 720))},
    TestParams{
        When(
            CameraProjection::equidistant,
            FisheyeCameraMount::table,
            /*panoFactor*/4,
            /*sourceCoords*/QPoint(1440, 360),
            /*cameraRollDegrees*/45,
            /*viewAngleDegrees*/QPointF(120, 30)),
        Expect(
            /*targetCoords*/QPoint(630, 276),
            /*targetFrameSize*/QSize(2880, 720))},
    TestParams{
        When(
            CameraProjection::equidistant,
            FisheyeCameraMount::table,
            /*panoFactor*/4,
            /*sourceCoords*/QPoint(720, 540),
            /*cameraRollDegrees*/45,
            /*viewAngleDegrees*/QPointF(120, 30)),
        Expect(
            /*targetCoords*/QPoint(642, 246),
            /*targetFrameSize*/QSize(2880, 720))},

    // Equirectangular view (360 degrees), Stereographic fisheye lens, Table mount.
    TestParams{
        When(
            CameraProjection::stereographic,
            FisheyeCameraMount::table,
            /*panoFactor*/4,
            /*sourceCoords*/QPoint(1440, 360),
            /*cameraRollDegrees*/0,
            /*viewAngleDegrees*/QPointF(0, 15)),
        Expect(
            /*targetCoords*/QPoint(1500, 78),
            /*targetFrameSize*/QSize(2880, 720))},
    TestParams{
        When(
            CameraProjection::stereographic,
            FisheyeCameraMount::table,
            /*panoFactor*/4,
            /*sourceCoords*/QPoint(720, 540),
            /*cameraRollDegrees*/0,
            /*viewAngleDegrees*/QPointF(0, 15)),
        Expect(
            /*targetCoords*/QPoint(1640, 71),
            /*targetFrameSize*/QSize(2880, 720))},
    TestParams{
        When(
            CameraProjection::stereographic,
            FisheyeCameraMount::table,
            /*panoFactor*/4,
            /*sourceCoords*/QPoint(1440, 360),
            /*cameraRollDegrees*/45,
            /*viewAngleDegrees*/QPointF(120, 30)),
        Expect(
            /*targetCoords*/QPoint(747, 284),
            /*targetFrameSize*/QSize(2880, 720))},
    TestParams{
        When(
            CameraProjection::stereographic,
            FisheyeCameraMount::table,
            /*panoFactor*/4,
            /*sourceCoords*/QPoint(720, 540),
            /*cameraRollDegrees*/45,
            /*viewAngleDegrees*/QPointF(120, 30)),
        Expect(
            /*targetCoords*/QPoint(752, 257),
            /*targetFrameSize*/QSize(2880, 720))},

    // Equirectangular view (360 degrees), Equisolid fisheye lens, Table mount.
    TestParams{
        When(
            CameraProjection::equisolid,
            FisheyeCameraMount::table,
            /*panoFactor*/4,
            /*sourceCoords*/QPoint(1440, 360),
            /*cameraRollDegrees*/0,
            /*viewAngleDegrees*/QPointF(0, 15)),
        Expect(
            /*targetCoords*/QPoint(1500, 46),
            /*targetFrameSize*/QSize(2880, 720))},
    TestParams{
        When(
            CameraProjection::equisolid,
            FisheyeCameraMount::table,
            /*panoFactor*/4,
            /*sourceCoords*/QPoint(720, 540),
            /*cameraRollDegrees*/0,
            /*viewAngleDegrees*/QPointF(0, 15)),
        Expect(
            /*targetCoords*/QPoint(1655, 43),
            /*targetFrameSize*/QSize(2880, 720))},
    TestParams{
        When(
            CameraProjection::equisolid,
            FisheyeCameraMount::table,
            /*panoFactor*/4,
            /*sourceCoords*/QPoint(1440, 360),
            /*cameraRollDegrees*/45,
            /*viewAngleDegrees*/QPointF(120, 30)),
        Expect(
            /*targetCoords*/QPoint(578, 272),
            /*targetFrameSize*/QSize(2880, 720))},
    TestParams{
        When(
            CameraProjection::equisolid,
            FisheyeCameraMount::table,
            /*panoFactor*/4,
            /*sourceCoords*/QPoint(720, 540),
            /*cameraRollDegrees*/45,
            /*viewAngleDegrees*/QPointF(120, 30)),
        Expect(
            /*targetCoords*/QPoint(593, 241),
            /*targetFrameSize*/QSize(2880, 720))},

    // Rectilinear view, Equidistant fisheye lens, Ceiling mount.
    TestParams{
        When(
            CameraProjection::equidistant,
            FisheyeCameraMount::ceiling,
            /*panoFactor*/1,
            /*sourceCoords*/QPoint(960, 540),
            /*cameraRollDegrees*/0,
            /*viewAngleDegrees*/QPointF(0, 15)),
        Expect(
            /*targetCoords*/QPoint(1000, 84))},
    TestParams{
        When(
            CameraProjection::equidistant,
            FisheyeCameraMount::ceiling,
            /*panoFactor*/1,
            /*sourceCoords*/QPoint(480, 810),
            /*cameraRollDegrees*/0,
            /*viewAngleDegrees*/QPointF(0, 15)),
        Expect(
            /*targetCoords*/QPoint(903, 110))},
    TestParams{
        When(
            CameraProjection::equidistant,
            FisheyeCameraMount::ceiling,
            /*panoFactor*/1,
            /*sourceCoords*/QPoint(960, 540),
            /*cameraRollDegrees*/45,
            /*viewAngleDegrees*/QPointF(120, 30)),
        Expect(
            /*targetCoords*/QPoint(422, 414))},
    TestParams{
        When(
            CameraProjection::equidistant,
            FisheyeCameraMount::ceiling,
            /*panoFactor*/1,
            /*sourceCoords*/QPoint(480, 810),
            /*cameraRollDegrees*/45,
            /*viewAngleDegrees*/QPointF(120, 30)),
        Expect(
            /*targetCoords*/QPoint(443, 467))},

    // Rectilinear view, Stereographic fisheye lens, Ceiling mount.
    TestParams{
        When(
            CameraProjection::stereographic,
            FisheyeCameraMount::ceiling,
            /*panoFactor*/1,
            /*sourceCoords*/QPoint(960, 540),
            /*cameraRollDegrees*/0,
            /*viewAngleDegrees*/QPointF(0, 15)),
        Expect(
            /*targetCoords*/QPoint(1000, 117))},
    TestParams{
        When(
            CameraProjection::stereographic,
            FisheyeCameraMount::ceiling,
            /*panoFactor*/1,
            /*sourceCoords*/QPoint(480, 810),
            /*cameraRollDegrees*/0,
            /*viewAngleDegrees*/QPointF(0, 15)),
        Expect(
            /*targetCoords*/QPoint(912, 148))},
    TestParams{
        When(
            CameraProjection::stereographic,
            FisheyeCameraMount::ceiling,
            /*panoFactor*/1,
            /*sourceCoords*/QPoint(960, 540),
            /*cameraRollDegrees*/45,
            /*viewAngleDegrees*/QPointF(120, 30)),
        Expect(
            /*targetCoords*/QPoint(499, 426))},
    TestParams{
        When(
            CameraProjection::stereographic,
            FisheyeCameraMount::ceiling,
            /*panoFactor*/1,
            /*sourceCoords*/QPoint(480, 810),
            /*cameraRollDegrees*/45,
            /*viewAngleDegrees*/QPointF(120, 30)),
        Expect(
            /*targetCoords*/QPoint(524, 472))},

    // Rectilinear view, Equisolid fisheye lens, Ceiling mount.
    TestParams{
        When(
            CameraProjection::equisolid,
            FisheyeCameraMount::ceiling,
            /*panoFactor*/1,
            /*sourceCoords*/QPoint(960, 540),
            /*cameraRollDegrees*/0,
            /*viewAngleDegrees*/QPointF(0, 15)),
        Expect(
            /*targetCoords*/QPoint(1000, 70))},
    TestParams{
        When(
            CameraProjection::equisolid,
            FisheyeCameraMount::ceiling,
            /*panoFactor*/1,
            /*sourceCoords*/QPoint(480, 810),
            /*cameraRollDegrees*/0,
            /*viewAngleDegrees*/QPointF(0, 15)),
        Expect(
            /*targetCoords*/QPoint(899, 94))},
    TestParams{
        When(
            CameraProjection::equisolid,
            FisheyeCameraMount::ceiling,
            /*panoFactor*/1,
            /*sourceCoords*/QPoint(960, 540),
            /*cameraRollDegrees*/45,
            /*viewAngleDegrees*/QPointF(120, 30)),
        Expect(
            /*targetCoords*/QPoint(386, 409))},
    TestParams{
        When(
            CameraProjection::equisolid,
            FisheyeCameraMount::ceiling,
            /*panoFactor*/1,
            /*sourceCoords*/QPoint(480, 810),
            /*cameraRollDegrees*/45,
            /*viewAngleDegrees*/QPointF(120, 30)),
        Expect(
            /*targetCoords*/QPoint(405, 465))},

    // Equirectangular view (180 degrees), Equidistant fisheye lens, Ceiling mount.
    TestParams{
        When(
            CameraProjection::equidistant,
            FisheyeCameraMount::ceiling,
            /*panoFactor*/2,
            /*sourceCoords*/QPoint(1016, 509),
            /*cameraRollDegrees*/0,
            /*viewAngleDegrees*/QPointF(0, 15)),
        Expect(
            /*targetCoords*/QPoint(1058, 79),
            /*targetFrameSize*/QSize(2032, 1018))},
    TestParams{
        When(
            CameraProjection::equidistant,
            FisheyeCameraMount::ceiling,
            /*panoFactor*/2,
            /*sourceCoords*/QPoint(508, 764),
            /*cameraRollDegrees*/0,
            /*viewAngleDegrees*/QPointF(0, 15)),
        Expect(
            /*targetCoords*/QPoint(960, 101),
            /*targetFrameSize*/QSize(2032, 1018))},
    TestParams{
        When(
            CameraProjection::equidistant,
            FisheyeCameraMount::ceiling,
            /*panoFactor*/2,
            /*sourceCoords*/QPoint(1016, 509),
            /*cameraRollDegrees*/45,
            /*viewAngleDegrees*/QPointF(120, 30)),
        Expect(
            /*targetCoords*/QPoint(445, 390),
            /*targetFrameSize*/QSize(2032, 1018))},
    TestParams{
        When(
            CameraProjection::equidistant,
            FisheyeCameraMount::ceiling,
            /*panoFactor*/2,
            /*sourceCoords*/QPoint(508, 764),
            /*cameraRollDegrees*/45,
            /*viewAngleDegrees*/QPointF(120, 30)),
        Expect(
            /*targetCoords*/QPoint(468, 433),
            /*targetFrameSize*/QSize(2032, 1018))},

    // Equirectangular view (180 degrees), Stereographic fisheye lens, Ceiling mount.
    TestParams{
        When(
            CameraProjection::stereographic,
            FisheyeCameraMount::ceiling,
            /*panoFactor*/2,
            /*sourceCoords*/QPoint(1016, 509),
            /*cameraRollDegrees*/0,
            /*viewAngleDegrees*/QPointF(0, 15)),
        Expect(
            /*targetCoords*/QPoint(1058, 110),
            /*targetFrameSize*/QSize(2032, 1018))},
    TestParams{
        When(
            CameraProjection::stereographic,
            FisheyeCameraMount::ceiling,
            /*panoFactor*/2,
            /*sourceCoords*/QPoint(508, 764),
            /*cameraRollDegrees*/0,
            /*viewAngleDegrees*/QPointF(0, 15)),
        Expect(
            /*targetCoords*/QPoint(969, 137),
            /*targetFrameSize*/QSize(2032, 1018))},
    TestParams{
        When(
            CameraProjection::stereographic,
            FisheyeCameraMount::ceiling,
            /*panoFactor*/2,
            /*sourceCoords*/QPoint(1016, 509),
            /*cameraRollDegrees*/45,
            /*viewAngleDegrees*/QPointF(120, 30)),
        Expect(
            /*targetCoords*/QPoint(527, 401),
            /*targetFrameSize*/QSize(2032, 1018))},
    TestParams{
        When(
            CameraProjection::stereographic,
            FisheyeCameraMount::ceiling,
            /*panoFactor*/2,
            /*sourceCoords*/QPoint(508, 764),
            /*cameraRollDegrees*/45,
            /*viewAngleDegrees*/QPointF(120, 30)),
        Expect(
            /*targetCoords*/QPoint(554, 438),
            /*targetFrameSize*/QSize(2032, 1018))},

    // Equirectangular view (180 degrees), Equisolid fisheye lens, Ceiling mount.
    TestParams{
        When(
            CameraProjection::equisolid,
            FisheyeCameraMount::ceiling,
            /*panoFactor*/2,
            /*sourceCoords*/QPoint(1016, 509),
            /*cameraRollDegrees*/0,
            /*viewAngleDegrees*/QPointF(0, 15)),
        Expect(
            /*targetCoords*/QPoint(1058, 66),
            /*targetFrameSize*/QSize(2032, 1018))},
    TestParams{
        When(
            CameraProjection::equisolid,
            FisheyeCameraMount::ceiling,
            /*panoFactor*/2,
            /*sourceCoords*/QPoint(508, 764),
            /*cameraRollDegrees*/0,
            /*viewAngleDegrees*/QPointF(0, 15)),
        Expect(
            /*targetCoords*/QPoint(956, 86),
            /*targetFrameSize*/QSize(2032, 1018))},
    TestParams{
        When(
            CameraProjection::equisolid,
            FisheyeCameraMount::ceiling,
            /*panoFactor*/2,
            /*sourceCoords*/QPoint(1016, 509),
            /*cameraRollDegrees*/45,
            /*viewAngleDegrees*/QPointF(120, 30)),
        Expect(
            /*targetCoords*/QPoint(408, 385),
            /*targetFrameSize*/QSize(2032, 1018))},
    TestParams{
        When(
            CameraProjection::equisolid,
            FisheyeCameraMount::ceiling,
            /*panoFactor*/2,
            /*sourceCoords*/QPoint(508, 764),
            /*cameraRollDegrees*/45,
            /*viewAngleDegrees*/QPointF(120, 30)),
        Expect(
            /*targetCoords*/QPoint(429, 430),
            /*targetFrameSize*/QSize(2032, 1018))},

    // Equirectangular view (360 degrees), Equidistant fisheye lens, Ceiling mount.
    TestParams{
        When(
            CameraProjection::equidistant,
            FisheyeCameraMount::ceiling,
            /*panoFactor*/4,
            /*sourceCoords*/QPoint(1440, 360),
            /*cameraRollDegrees*/0,
            /*viewAngleDegrees*/QPointF(0, 15)),
        Expect(
            /*targetCoords*/QPoint(1500, 56),
            /*targetFrameSize*/QSize(2880, 720))},
    TestParams{
        When(
            CameraProjection::equidistant,
            FisheyeCameraMount::ceiling,
            /*panoFactor*/4,
            /*sourceCoords*/QPoint(720, 540),
            /*cameraRollDegrees*/0,
            /*viewAngleDegrees*/QPointF(0, 15)),
        Expect(
            /*targetCoords*/QPoint(1357, 65),
            /*targetFrameSize*/QSize(2880, 720))},
    TestParams{
        When(
            CameraProjection::equidistant,
            FisheyeCameraMount::ceiling,
            /*panoFactor*/4,
            /*sourceCoords*/QPoint(1440, 360),
            /*cameraRollDegrees*/45,
            /*viewAngleDegrees*/QPointF(120, 30)),
        Expect(
            /*targetCoords*/QPoint(630, 276),
            /*targetFrameSize*/QSize(2880, 720))},
    TestParams{
        When(
            CameraProjection::equidistant,
            FisheyeCameraMount::ceiling,
            /*panoFactor*/4,
            /*sourceCoords*/QPoint(720, 540),
            /*cameraRollDegrees*/45,
            /*viewAngleDegrees*/QPointF(120, 30)),
        Expect(
            /*targetCoords*/QPoint(635, 305),
            /*targetFrameSize*/QSize(2880, 720))},

    // Equirectangular view (360 degrees), Stereographic fisheye lens, Ceiling mount.
    TestParams{
        When(
            CameraProjection::stereographic,
            FisheyeCameraMount::ceiling,
            /*panoFactor*/4,
            /*sourceCoords*/QPoint(1440, 360),
            /*cameraRollDegrees*/0,
            /*viewAngleDegrees*/QPointF(0, 15)),
        Expect(
            /*targetCoords*/QPoint(1500, 78),
            /*targetFrameSize*/QSize(2880, 720))},
    TestParams{
        When(
            CameraProjection::stereographic,
            FisheyeCameraMount::ceiling,
            /*panoFactor*/4,
            /*sourceCoords*/QPoint(720, 540),
            /*cameraRollDegrees*/0,
            /*viewAngleDegrees*/QPointF(0, 15)),
        Expect(
            /*targetCoords*/QPoint(1369, 88),
            /*targetFrameSize*/QSize(2880, 720))},
    TestParams{
        When(
            CameraProjection::stereographic,
            FisheyeCameraMount::ceiling,
            /*panoFactor*/4,
            /*sourceCoords*/QPoint(1440, 360),
            /*cameraRollDegrees*/45,
            /*viewAngleDegrees*/QPointF(120, 30)),
        Expect(
            /*targetCoords*/QPoint(747, 284),
            /*targetFrameSize*/QSize(2880, 720))},
    TestParams{
        When(
            CameraProjection::stereographic,
            FisheyeCameraMount::ceiling,
            /*panoFactor*/4,
            /*sourceCoords*/QPoint(720, 540),
            /*cameraRollDegrees*/45,
            /*viewAngleDegrees*/QPointF(120, 30)),
        Expect(
            /*targetCoords*/QPoint(756, 309),
            /*targetFrameSize*/QSize(2880, 720))},

    // Equirectangular view (360 degrees), Equisolid fisheye lens, Ceiling mount.
    TestParams{
        When(
            CameraProjection::equisolid,
            FisheyeCameraMount::ceiling,
            /*panoFactor*/4,
            /*sourceCoords*/QPoint(1440, 360),
            /*cameraRollDegrees*/0,
            /*viewAngleDegrees*/QPointF(0, 15)),
        Expect(
            /*targetCoords*/QPoint(1500, 46),
            /*targetFrameSize*/QSize(2880, 720))},
    TestParams{
        When(
            CameraProjection::equisolid,
            FisheyeCameraMount::ceiling,
            /*panoFactor*/4,
            /*sourceCoords*/QPoint(720, 540),
            /*cameraRollDegrees*/0,
            /*viewAngleDegrees*/QPointF(0, 15)),
        Expect(
            /*targetCoords*/QPoint(1351, 55),
            /*targetFrameSize*/QSize(2880, 720))},
    TestParams{
        When(
            CameraProjection::equisolid,
            FisheyeCameraMount::ceiling,
            /*panoFactor*/4,
            /*sourceCoords*/QPoint(1440, 360),
            /*cameraRollDegrees*/45,
            /*viewAngleDegrees*/QPointF(120, 30)),
        Expect(
            /*targetCoords*/QPoint(578, 272),
            /*targetFrameSize*/QSize(2880, 720))},
    TestParams{
        When(
            CameraProjection::equisolid,
            FisheyeCameraMount::ceiling,
            /*panoFactor*/4,
            /*sourceCoords*/QPoint(720, 540),
            /*cameraRollDegrees*/45,
            /*viewAngleDegrees*/QPointF(120, 30)),
        Expect(
            /*targetCoords*/QPoint(580, 303),
            /*targetFrameSize*/QSize(2880, 720))},

    // Rectilinear view, 360VR panoramic camera.
    TestParams{
        When(
            /*panoFactor*/1,
            /*sourceCoords*/QPoint(960, 540)),
        Expect(
            /*targetCoords*/QPoint(960, 543),
            /*targetFrameSize*/QSize(1920, 1086))},
    TestParams{
        When(
            /*panoFactor*/1,
            /*sourceCoords*/QPoint(480, 810)),
        Expect(
            /*targetCoords*/QPoint(919, 566),
            /*targetFrameSize*/QSize(1920, 1086))},
    TestParams{
        When(
            /*panoFactor*/1,
            /*sourceCoords*/QPoint(960, 540),
            /*horizonCorrectionDegrees*/QPointF(80, -50),
            /*viewAngleDegrees*/QPointF(-40, 30)),
        Expect(
            /*targetCoords*/QPoint(949, 297),
            /*targetFrameSize*/QSize(1920, 1086))},
    TestParams{
        When(
            /*panoFactor*/1,
            /*sourceCoords*/QPoint(480, 810),
            /*horizonCorrectionDegrees*/QPointF(80, -50),
            /*viewAngleDegrees*/QPointF(-40, 30)),
        Expect(
            /*targetCoords*/QPoint(896, 270),
            /*targetFrameSize*/QSize(1920, 1086))},

    // Equirectangular view (180 degrees), 360VR panoramic camera.
    TestParams{
        When(
            /*panoFactor*/2,
            /*sourceCoords*/QPoint(1024, 512)),
        Expect(
            /*targetCoords*/QPoint(1024, 512),
            /*targetFrameSize*/QSize(2048, 1024))},
    TestParams{
        When(
            /*panoFactor*/2,
            /*sourceCoords*/QPoint(480, 810)),
        Expect(
            /*targetCoords*/QPoint(979, 537),
            /*targetFrameSize*/QSize(2048, 1024))},
    TestParams{
        When(
            /*panoFactor*/2,
            /*sourceCoords*/QPoint(1024, 512),
            /*horizonCorrectionDegrees*/QPointF(80, -50),
            /*viewAngleDegrees*/QPointF(-40, 30)),
        Expect(
            /*targetCoords*/QPoint(1012, 280),
            /*targetFrameSize*/QSize(2048, 1024))},
    TestParams{
        When(
            /*panoFactor*/2,
            /*sourceCoords*/QPoint(480, 810),
            /*horizonCorrectionDegrees*/QPointF(80, -50),
            /*viewAngleDegrees*/QPointF(-40, 30)),
        Expect(
            /*targetCoords*/QPoint(956, 258),
            /*targetFrameSize*/QSize(2048, 1024))},

    // Equirectangular view (360 degrees), 360VR panoramic camera.
    TestParams{
        When(
            /*panoFactor*/4,
            /*sourceCoords*/QPoint(1448, 362)),
        Expect(
            /*targetCoords*/QPoint(1448, 362),
            /*targetFrameSize*/QSize(2896, 724))},
    TestParams{
        When(
            /*panoFactor*/4,
            /*sourceCoords*/QPoint(480, 810)),
        Expect(
            /*targetCoords*/QPoint(1367, 381),
            /*targetFrameSize*/QSize(2896, 724))},
    TestParams{
        When(
            /*panoFactor*/4,
            /*sourceCoords*/QPoint(1448, 362),
            /*horizonCorrectionDegrees*/QPointF(80, -50),
            /*viewAngleDegrees*/QPointF(-40, 30)),
        Expect(
            /*targetCoords*/QPoint(1431, 198),
            /*targetFrameSize*/QSize(2896, 724))},
    TestParams{
        When(
            /*panoFactor*/4,
            /*sourceCoords*/QPoint(480, 810),
            /*horizonCorrectionDegrees*/QPointF(80, -50),
            /*viewAngleDegrees*/QPointF(-40, 30)),
        Expect(
            /*targetCoords*/QPoint(1337, 177),
            /*targetFrameSize*/QSize(2896, 724))},
    });

    INSTANTIATE_TEST_SUITE_P(DewarpingImageFilterTest, DewarpingImageFilterTest,
        ::testing::ValuesIn(kDewarpingImageFilterTestParams));


TEST(DewarpingImageFilterTest, bilinearFiltering)
{
    static const int kStride = 64;
    static const int kHeight = 4;
    uint8_t buffer[kStride * kHeight];

    static const int kX = 2;
    static const int kY = 1;
    int kOffset = kX + kStride * kY;

    buffer[kOffset] = 0;
    buffer[kOffset + 1] = 255;
    buffer[kOffset + kStride] = 255;
    buffer[kOffset + kStride + 1] = 255;

    uint8_t pixel = QnDewarpingImageFilter::GetPixel(buffer, kStride, kX, kY, kStride, kHeight);
    ASSERT_EQ(0, pixel);

    pixel = QnDewarpingImageFilter::GetPixel(buffer, kStride, kX + 1, kY + 1, kStride, kHeight);
    ASSERT_EQ(256 - 1, pixel);

    pixel = QnDewarpingImageFilter::GetPixel(buffer, kStride, kX + 0.5, kY, kStride, kHeight);
    ASSERT_EQ(128 - 1, pixel);

    pixel = QnDewarpingImageFilter::GetPixel(buffer, kStride, kX, kY + 0.5, kStride, kHeight);
    ASSERT_EQ(128 - 1, pixel);

    pixel = QnDewarpingImageFilter::GetPixel(buffer, kStride, kX + 0.5, kY + 0.5, kStride, kHeight);
    ASSERT_EQ(128 + 64 - 1, pixel);

    pixel = QnDewarpingImageFilter::GetPixel(buffer, kStride, kX + 0.5, kY + 0.75, kStride, kHeight);
    ASSERT_EQ(128 + 64 + 32 - 1, pixel);
}

} // namespace transcoding::filters::test
