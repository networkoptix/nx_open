#include "viewport_ptz_controller.h"

#include <QtCore/QtMath>
#include <QtGui/QVector3D>

#include <utils/common/warnings.h>
#include <utils/math/coordinate_transformations.h>

QnViewportPtzController::QnViewportPtzController(const QnPtzControllerPtr &baseController):
    base_type(baseController)
{
    if(!baseController->hasCapabilities(Qn::FlipPtzCapability))
        qnWarning("Base controller doesn't have a Qn::FlipPtzCapability. Flip will not be taken into account by advanced PTZ.");
    if(!baseController->hasCapabilities(Qn::LimitsPtzCapability))
        qnWarning("Base controller doesn't have a Qn::LimitsPtzCapability. Position caching may not work for advanced PTZ.");
}

bool QnViewportPtzController::extends(const QnPtzControllerPtr &baseController) {
    return 
        baseController->hasCapabilities(Qn::AbsolutePtzCapabilities | Qn::LogicalPositioningPtzCapability) &&
        !baseController->hasCapabilities(Qn::ViewportPtzCapability);
}

Qn::PtzCapabilities QnViewportPtzController::getCapabilities() {
    return base_type::getCapabilities() | Qn::ViewportPtzCapability;
}

bool QnViewportPtzController::viewportMove(qreal aspectRatio, const QRectF &viewport, qreal speed) {
    QVector3D oldPosition;
    if(!getPosition(Qn::LogicalPtzCoordinateSpace, &oldPosition))
        return false;

    /* Note that we don't care about getLimits result as default-constructed
     * limits is actually 'no limits'. */
    QnPtzLimits limits;
    getLimits(Qn::LogicalPtzCoordinateSpace, &limits); 

    /* Same here, we don't care about getFlip result. */
    Qt::Orientations flip = 0;
    getFlip(&flip);

    /* Passed viewport should be square, but who knows... */
    float zoom = 1.0 / qMax(viewport.width(), viewport.height()); /* For 2x zoom we'll get 2.0 here. */

    /* AR-adjusted position delta in viewport space. */
    QVector2D delta = QVector2D(viewport.center()) - QVector2D(0.5, 0.5);
    delta.setY(delta.y() / aspectRatio);

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
    float unit = std::tan(qDegreesToRadians(oldPosition.z()) / 2.0) * 2.0;
    QVector3D r = sphericalToCartesian<QVector3D>(1.0, qDegreesToRadians(oldPosition.x()), qDegreesToRadians(oldPosition.y()));
    QVector3D x = sphericalToCartesian<QVector3D>(1.0, qDegreesToRadians(oldPosition.x() + 90 + (flip & Qt::Horizontal ? 180 : 0)), 0.0) * unit;
    QVector3D y = sphericalToCartesian<QVector3D>(1.0, qDegreesToRadians(oldPosition.x()), qDegreesToRadians(oldPosition.y() - 90 + (flip & Qt::Vertical ? 180 : 0))) * unit;

    /* Calculate new position in spherical coordinates. */
    QVector3D r1 = r + x * delta.x() + y * delta.y();
    QnSphericalPoint<float> newSpherical = cartesianToSpherical<QVector3D>(r1);

    /* Calculate new FOV. */
    float newFov = qRadiansToDegrees(std::atan((unit / 2.0) / zoom)) * 2.0;
    
    /* Fill in new position. */
    QVector3D newPosition = qBound(QVector3D(qRadiansToDegrees(newSpherical.phi), qRadiansToDegrees(newSpherical.psi), newFov), limits);

    /* Send it to the camera. */
    return absoluteMove(Qn::LogicalPtzCoordinateSpace, newPosition, speed);
}
