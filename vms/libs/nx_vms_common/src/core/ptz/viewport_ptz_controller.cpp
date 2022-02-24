// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "viewport_ptz_controller.h"

#include <QtCore/QtMath>
#include <QtGui/QVector3D>

#include <utils/math/coordinate_transformations.h>

namespace {

enum class Projection
{
    Rectilinear,
    Equirectangular
};

} // namespace

QnViewportPtzController::QnViewportPtzController(const QnPtzControllerPtr &baseController):
    base_type(baseController)
{
    // TODO: #sivanov Handle finished properly.

#ifdef PTZ_DEBUG
    if(!baseController->hasCapabilities(Ptz::FlipPtzCapability))
        NX_WARNING(this, "Base controller doesn't have a Ptz::FlipPtzCapability. Flip will not be taken into account by advanced PTZ.");
    if(!baseController->hasCapabilities(Ptz::LimitsPtzCapability))
        NX_WARNING(this, "Base controller doesn't have a Ptz::LimitsPtzCapability. Position caching may not work for advanced PTZ.");
#endif
}

bool QnViewportPtzController::extends(Ptz::Capabilities capabilities) {
    return (capabilities & Ptz::AbsolutePtzCapabilities) == Ptz::AbsolutePtzCapabilities
        && capabilities.testFlag(Ptz::LogicalPositioningPtzCapability)
        && !capabilities.testFlag(Ptz::ViewportPtzCapability);
}

Ptz::Capabilities QnViewportPtzController::getCapabilities(
    const Options& options) const
{
    Ptz::Capabilities capabilities = base_type::getCapabilities(options);
    return extends(capabilities) ? (capabilities | Ptz::ViewportPtzCapability) : capabilities;
}

bool QnViewportPtzController::viewportMove(
    qreal aspectRatio,
    const QRectF& viewport,
    qreal speed,
    const Options& options)
{
    Vector oldPosition;
    if(!getPosition(&oldPosition, CoordinateSpace::logical, options))
        return false;

    /* Note that we don't care about getLimits result as default-constructed
     * limits is actually 'no limits'. */
    QnPtzLimits limits;
    getLimits(&limits, CoordinateSpace::logical, options);

    // In theory projection should be a part of controller's interface.
    Projection projection = Projection::Rectilinear;
    if(limits.maxFov > 180.0 || qFuzzyCompare(limits.maxFov, 180.0))
        projection = Projection::Equirectangular;

    /* Same here, we don't care about getFlip result. */
    Qt::Orientations flip;
    getFlip(&flip, options);

    /* Passed viewport should be square, but who knows... */
    float zoom = 1.0 / qMax(viewport.width(), viewport.height()); /* For 2x zoom we'll get 2.0 here. */

    /* AR-adjusted position delta in viewport space. */
    QVector2D delta = QVector2D(viewport.center()) - QVector2D(0.5, 0.5);
    delta.setY(delta.y() / aspectRatio);

    if(projection == Projection::Rectilinear)
    {
        /* Viewport space to 3D conversion base.
         *
         *     unit
         * <----------->
         *
         * +-----+-----+   ^
         *  \    |    /    |
         *   \   |   /     |
         *    \  |  /      | 1.0
         *     \ | /       |
         *      \|/        |
         *       +         v
         *
         *      <->
         *      fov
         *     angle
         */
        float unit = std::tan(qDegreesToRadians(oldPosition.zoom) / 2.0) * 2.0;

        QVector3D r = sphericalToCartesian<QVector3D>(1.0, qDegreesToRadians(oldPosition.pan), qDegreesToRadians(oldPosition.tilt));
        QVector3D x = sphericalToCartesian<QVector3D>(1.0, qDegreesToRadians(oldPosition.pan + 90 + (flip & Qt::Horizontal ? 180 : 0)), 0.0) * unit;
        QVector3D y = sphericalToCartesian<QVector3D>(1.0, qDegreesToRadians(oldPosition.pan), qDegreesToRadians(oldPosition.tilt - 90 + (flip & Qt::Vertical ? 180 : 0))) * unit;

        /* Calculate new position in spherical coordinates. */
        QVector3D r1 = r + x * delta.x() + y * delta.y();
        QnSphericalPoint<float> newSpherical = cartesianToSpherical<QVector3D>(r1);

        /* Calculate new position. */
        float newFov = qRadiansToDegrees(std::atan((unit / 2.0) / zoom)) * 2.0;
        float newPan = qRadiansToDegrees(newSpherical.phi);
        float newTilt = qRadiansToDegrees(newSpherical.psi);
        Vector newPosition(newPan, newTilt, 0.0, newFov);

        /* Send it to the camera. */
        return absoluteMove(CoordinateSpace::logical, newPosition, speed, options);
    } else {
        /* Calculate new position. */
        float newPan = oldPosition.pan + oldPosition.zoom * delta.x();
        float newTilt = oldPosition.tilt - oldPosition.zoom * delta.y();
        float newFov = oldPosition.zoom / zoom;
        auto newPosition = qBound(Vector(newPan, newTilt, 0.0, newFov), limits);

        /* Send it to the camera. */
        return absoluteMove(CoordinateSpace::logical, newPosition, speed, options);
    }
}
