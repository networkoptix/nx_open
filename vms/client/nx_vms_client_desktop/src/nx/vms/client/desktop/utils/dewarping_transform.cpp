// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "dewarping_transform.h"

#include <QtCore/QtMath>
#include <QtGui/QMatrix4x4>
#include <QtGui/QTransform>
#include <QtGui/QVector3D>

#include <nx/utils/math/math.h>
#include <nx/vms/api/data/dewarping_data.h>
#include <nx/vms/client/core/utils/geometry.h>
#include <utils/common/aspect_ratio.h>

#include <cmath>

namespace {

static constexpr QRectF kUnitRect(0, 0, 1, 1);
static constexpr QVector3D kXAxis(1, 0, 0);
static constexpr QVector3D kYAxis(0, 1, 0);
static constexpr QVector3D kZAxis(0, 0, 1);

QPointF cartesianToSpherical(const QVector3D& cartesian)
{
    if (qFuzzyIsNull(cartesian.length()))
        return QPointF();

    const auto theta = atan2(cartesian.x(), cartesian.y());
    const auto phi = asin(cartesian.normalized().z());

    return QPointF(theta, phi);
}

QVector3D sphericalToCartesian(const QPointF& spherical)
{
    const auto theta = spherical.x();
    const auto phi = spherical.y();
    return QVector3D(
        sin(theta) * cos(phi),
        cos(theta) * cos(phi),
        sin(phi));
}

double wrapValue(double value, double min, double max)
{
    if (min > max)
        std::swap(min, max);

    if (qFuzzyBetween(min, value, max))
        return value;

    if (qFuzzyEquals(min, max))
        return min;

    const auto remainder = std::fmod(value - min, max - min);
    if (qFuzzyIsNull(remainder))
        return min;

    return remainder > 0.0
        ? remainder + min
        : remainder + max;
}

} // namespace

using namespace nx::vms::api;
using namespace nx::vms::client::core;

namespace nx::vms::client::desktop {

//-------------------------------------------------------------------------------------------------
// DewarpingTransform::Private declaration.

struct DewarpingTransform::Private
{
    DewarpingTransform* const q;

    const dewarping::MediaData mediaParams;
    const dewarping::ViewData itemParams;
    const QnAspectRatio frameAspectRatio;

    dewarping::ViewProjection viewProjectionType() const;
    bool isHorizontallyMountedFisheyeCamera() const;

    QTransform fromNormalizedFrameSpace() const;
    QTransform toViewSpace() const;

    QMatrix4x4 horizonCorrectionTransform() const;
    QMatrix4x4 sphereRotationTransform() const;

    QPointF cameraProject(const QVector3D& pointOnSphere) const;
    QVector3D cameraUnproject(const QPointF& imageCoords) const;

    QMatrix4x4 viewUnprojectionTransform() const;

    QPointF viewProject(const QVector3D& pointOnSphere) const;
    QVector3D viewUnproject(const QPointF& viewPoint) const;
};

//-------------------------------------------------------------------------------------------------
// CameraHotspotEditorWidget::Private definition.

dewarping::ViewProjection DewarpingTransform::Private::viewProjectionType() const
{
    return itemParams.panoFactor > 1.0
        ? dewarping::ViewProjection::equirectangular
        : dewarping::ViewProjection::rectilinear;
}

bool DewarpingTransform::Private::isHorizontallyMountedFisheyeCamera() const
{
    return dewarping::MediaData::isFisheye(mediaParams.cameraProjection)
        && mediaParams.viewMode != dewarping::FisheyeCameraMount::wall;
}

QTransform DewarpingTransform::Private::fromNormalizedFrameSpace() const
{
    QTransform transform;

    if (dewarping::MediaData::is360VR(mediaParams.cameraProjection))
    {
        transform.translate(0.5, 0.5);
        transform.scale(0.5 * M_1_PI, 0.5 * M_1_PI * frameAspectRatio.toFloat());
    }
    else if (dewarping::MediaData::isFisheye(mediaParams.cameraProjection))
    {
        const auto cameraRollAngle = isHorizontallyMountedFisheyeCamera()
            ? mediaParams.fovRot + 180.0
            : mediaParams.fovRot;

        transform.translate(mediaParams.xCenter, mediaParams.yCenter);
        transform.scale(
            mediaParams.radius,
            mediaParams.radius * frameAspectRatio.toFloat() / mediaParams.hStretch);
        transform.rotate(cameraRollAngle);
    }
    else
    {
        NX_ASSERT(false, "Unknown camera projection");
    }

    return transform;
}

QTransform DewarpingTransform::Private::toViewSpace() const
{
    QTransform transform;

    switch (viewProjectionType())
    {
        case dewarping::ViewProjection::equirectangular:
        {
            transform.translate(wrapValue(itemParams.xAngle, -M_PI, M_PI), -itemParams.yAngle);
            transform.scale(itemParams.fov, itemParams.fov / itemParams.panoFactor);
            transform.translate(-0.5, -0.5);
            break;
        }

        case dewarping::ViewProjection::rectilinear:
        {
            transform.scale(2.0, 2.0f);
            transform.translate(-0.5, isHorizontallyMountedFisheyeCamera() ? -1.0 : -0.5);
            break;
        }

        default:
            NX_ASSERT(false, "Unknown view projection");
            break;
    }

    if (isHorizontallyMountedFisheyeCamera()
        && mediaParams.viewMode == dewarping::FisheyeCameraMount::ceiling)
    {
        QTransform viewportRotateTransform(-1, 0, 0, 0, -1, 0, 1, 1, 1);
        transform = viewportRotateTransform * transform;
    }

    return transform;
}

QMatrix4x4 DewarpingTransform::Private::horizonCorrectionTransform() const
{
    QMatrix4x4 transform;

    transform.rotate(-mediaParams.sphereAlpha, kZAxis);
    transform.rotate(-mediaParams.sphereBeta, kXAxis);
    transform.rotate(mediaParams.sphereAlpha, kZAxis);

    return transform;
}

QMatrix4x4 DewarpingTransform::Private::sphereRotationTransform() const
{
    QMatrix4x4 transform;

    if (isHorizontallyMountedFisheyeCamera())
        transform.rotate(90.0, kXAxis);

    return transform;
}

QPointF DewarpingTransform::Private::cameraProject(const QVector3D& pointOnSphere) const
{
    const QPointF xz(pointOnSphere.x(), pointOnSphere.z());
    const auto y = pointOnSphere.y();

    switch (mediaParams.cameraProjection)
    {
        case dewarping::CameraProjection::equidistant:
        {
            const auto theta = acos(std::clamp(y, -1.0f, 1.0f));
            return qFuzzyIsNull(Geometry::length(xz))
                ? QPointF()
                : Geometry::normalized(xz) * theta / M_PI_2;
        }

        case dewarping::CameraProjection::stereographic:
            return xz / (1.0 + y);

        case dewarping::CameraProjection::equisolid:
            return xz / sqrt(1.0 + y);

        case dewarping::CameraProjection::equirectangular360:
            return cartesianToSpherical(horizonCorrectionTransform().map(pointOnSphere));

        default:
            NX_ASSERT(false, "Unknown camera projection");
            return {};
    }
}

QVector3D DewarpingTransform::Private::cameraUnproject(const QPointF& normalizedFramePoint) const
{
    switch (mediaParams.cameraProjection)
    {
        case dewarping::CameraProjection::equidistant:
        {
            const auto r = Geometry::length(normalizedFramePoint);
            if (qFuzzyIsNull(r))
                return QVector3D(0.0, 1.0, 0.0);
            const auto theta = r * M_PI_2;
            const auto xz = normalizedFramePoint * (sin(theta) / r);
            return QVector3D(xz.x(), cos(theta), xz.y());
        }

        case dewarping::CameraProjection::stereographic:
        {
            const auto rSquared = Geometry::lengthSquared(normalizedFramePoint);
            const auto xz = normalizedFramePoint * 2.0;
            return QVector3D(xz.x(), 1.0 - rSquared, xz.y()) * (1.0 / (rSquared + 1.0));
        }

        case dewarping::CameraProjection::equisolid:
        {
            const auto rSquared = Geometry::lengthSquared(normalizedFramePoint);
            const auto xz = normalizedFramePoint * sqrt(2.0 - rSquared);
            return QVector3D(xz.x(), 1.0 - rSquared,  xz.y());
        }

        case dewarping::CameraProjection::equirectangular360:
        {
            const auto pointOnSphere = sphericalToCartesian(normalizedFramePoint);
            return horizonCorrectionTransform().inverted().map(pointOnSphere);
        }

        default:
            NX_ASSERT(false, "Unknown camera projection");
            return {};
    }
}

QPointF DewarpingTransform::Private::viewProject(const QVector3D& pointOnSphere) const
{
    switch (viewProjectionType())
    {
        case dewarping::ViewProjection::rectilinear:
        {
            const auto projectedPoint = viewUnprojectionTransform().inverted().map(pointOnSphere);
            QPointF viewPoint(projectedPoint.x(), projectedPoint.y());
            viewPoint = !qFuzzyIsNull(projectedPoint.z())
                ? viewPoint / projectedPoint.z()
                : viewPoint * std::numeric_limits<qreal>::infinity();
            return viewPoint;
        }

        case dewarping::ViewProjection::equirectangular:
            return cartesianToSpherical(sphereRotationTransform().inverted().map(pointOnSphere));

        default:
            NX_ASSERT(false, "Unknown view projection");
            return {};
    }
}

QVector3D DewarpingTransform::Private::viewUnproject(const QPointF& viewPoint) const
{
    switch (viewProjectionType())
    {
        case dewarping::ViewProjection::rectilinear:
            return viewUnprojectionTransform()
                .map(QVector3D(viewPoint.x(), viewPoint.y(), 1.0)).normalized();

        case dewarping::ViewProjection::equirectangular:
            return sphereRotationTransform().map(sphericalToCartesian(viewPoint));

        default:
            NX_ASSERT(false, "Unknown view projection");
            return {};
    }
}

QMatrix4x4 DewarpingTransform::Private::viewUnprojectionTransform() const
{
    const auto yawAngle = isHorizontallyMountedFisheyeCamera() ? 0.0 : itemParams.xAngle;
    const auto pitchAngle = isHorizontallyMountedFisheyeCamera()
        ? itemParams.yAngle - M_PI_2 - (itemParams.fov / frameAspectRatio.toFloat()) / 2.0
        : itemParams.yAngle;
    const auto rollAngle = isHorizontallyMountedFisheyeCamera() ? -itemParams.xAngle : 0.0;

    const auto kx = tan(itemParams.fov / 2.0);
    const auto ky = kx / frameAspectRatio.toFloat();

    QMatrix4x4 transform;
    std::swap(transform(1, 1), transform(1, 2));
    std::swap(transform(2, 1), transform(2, 2));

    transform.rotate(qRadiansToDegrees(rollAngle), QVector3D(kZAxis));
    transform.rotate(qRadiansToDegrees(yawAngle), QVector3D(kYAxis));
    transform.rotate(qRadiansToDegrees(pitchAngle), QVector3D(kXAxis));
    transform.scale(kx, ky);

    return transform;
}

//-------------------------------------------------------------------------------------------------
// CameraHotspotEditorWidget definition.

DewarpingTransform::DewarpingTransform(
    const dewarping::MediaData& mediaParams,
    const dewarping::ViewData& itemParams,
    const QnAspectRatio& frameAspectRatio)
    :
    d(new Private{this, mediaParams, itemParams, frameAspectRatio})
{
}

DewarpingTransform::~DewarpingTransform()
{
}

std::optional<QPointF> DewarpingTransform::mapToFrame(const QPointF& unitRectViewPoint) const
{
    const auto viewPoint = d->toViewSpace().map(unitRectViewPoint);
    const auto spherePoint = d->viewUnproject(viewPoint);
    const auto normalizedFramePoint = d->cameraProject(spherePoint);
    const auto unitRectFramePoint = d->fromNormalizedFrameSpace().map(normalizedFramePoint);

    return kUnitRect.contains(unitRectFramePoint)
        ? std::make_optional(unitRectFramePoint)
        : std::nullopt;
}

std::optional<QPointF> DewarpingTransform::mapToView(const QPointF& framePoint) const
{
    const auto normalizedFramePoint = d->fromNormalizedFrameSpace().inverted().map(framePoint);
    const auto spherePoint = d->cameraUnproject(normalizedFramePoint);
    const auto viewPoint = d->viewProject(spherePoint);
    auto unitRectViewPoint = d->toViewSpace().inverted().map(viewPoint);

    if (d->mediaParams.is360VR() || d->isHorizontallyMountedFisheyeCamera())
    {
        if (d->viewProjectionType() == dewarping::ViewProjection::equirectangular)
        {
            unitRectViewPoint.rx() = wrapValue(unitRectViewPoint.x(),
                0.5 - M_PI / d->itemParams.fov,
                0.5 + M_PI / d->itemParams.fov);
        }
        else if (d->viewProjectionType() == dewarping::ViewProjection::rectilinear)
        {
            if (QVector3D::dotProduct(spherePoint,
                sphericalToCartesian({d->itemParams.xAngle, -d->itemParams.yAngle})) < 0)
            {
                return std::nullopt; //< Projected from behind, not a valid result.
            }
        }
    }

    return kUnitRect.contains(unitRectViewPoint)
        ? std::make_optional(unitRectViewPoint)
        : std::nullopt;
}

} // namespace nx::vms::client::desktop
