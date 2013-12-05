#include "relative_ptz_controller.h"

#include <QtCore/QtMath>
#include <QtGui/QVector3D>

#include <utils/math/coordinate_transformations.h>

QnViewportPtzController::QnViewportPtzController(const QnPtzControllerPtr &baseController):
    base_type(baseController) 
{}

bool QnViewportPtzController::extends(const QnPtzControllerPtr &baseController) {
    return 
        baseController->hasCapabilities(Qn::AbsolutePtzCapabilities | Qn::LogicalCoordinateSpaceCapability) &&
        !baseController->hasCapabilities(Qn::ViewportCoordinateSpaceCapability);
}

Qn::PtzCapabilities QnViewportPtzController::getCapabilities() {
    return base_type::getCapabilities() | Qn::ViewportCoordinateSpaceCapability;
}

bool QnViewportPtzController::relativeMove(qreal aspectRatio, const QRectF &viewport) {
    // TODO: #Elric cache it!
    QnPtzLimits limits;
    if(!getLimits(&limits))
        return false;

    // TODO: #Elric cache it!
    QVector3D oldPosition;
    if(!getPosition(Qn::LogicalCoordinateSpace, &oldPosition))
        return false;

    // TODO: #Elric also take flip into account.

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
    QVector3D x = sphericalToCartesian<QVector3D>(1.0, qDegreesToRadians(oldPosition.x() + 90), 0.0) * unit;
    QVector3D y = sphericalToCartesian<QVector3D>(1.0, qDegreesToRadians(oldPosition.x()), qDegreesToRadians(oldPosition.y() - 90)) * unit;

    /* Calculate new position in spherical coordinates. */
    QVector3D r1 = r + x * delta.x() + y * delta.y();
    QnSphericalPoint<float> newSpherical = cartesianToSpherical<QVector3D>(r1);

    /* Calculate new FOV. */
    float newFov = qRadiansToDegrees(std::atan((unit / 2.0) / zoom)) * 2.0;
    
    /* Fill in new position. */
    QVector3D newPosition = QVector3D(qRadiansToDegrees(newSpherical.phi), qRadiansToDegrees(newSpherical.psi), newFov);
    newPosition.setX(qBound<float>(limits.minPan, newPosition.x(), limits.maxPan));
    newPosition.setY(qBound<float>(limits.minTilt, newPosition.y(), limits.maxTilt));
    newPosition.setZ(qBound<float>(limits.minFov, newPosition.z(), limits.maxFov));

    /* Send it to the camera. */
    return absoluteMove(Qn::LogicalCoordinateSpace, newPosition);
}
