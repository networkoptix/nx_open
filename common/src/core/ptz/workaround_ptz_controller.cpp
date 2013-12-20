#include "workaround_ptz_controller.h"

#include <utils/math/math.h>
#include <utils/math/coordinate_transformations.h>

#include <common/common_module.h>
#include <core/resource/resource_data.h>
#include <core/resource/camera_resource.h>
#include <core/resource_managment/resource_data_pool.h>

QnWorkaroundPtzController::QnWorkaroundPtzController(const QnPtzControllerPtr &baseController):
    base_type(baseController),
    m_octagonal(false)
{
    QnVirtualCameraResourcePtr camera = resource().dynamicCast<QnVirtualCameraResource>();
    if(!camera)
        return;

    m_octagonal = qnCommon->dataPool()->data(camera).value<bool>(lit("octagonalPtz"), false);

    // TODO: #Elric propert finished handling.
}

bool QnWorkaroundPtzController::continuousMove(const QVector3D &speed) {
    if(!m_octagonal) {
        return base_type::continuousMove(speed);
    } else {
        QVector2D cartesianSpeed(speed);
        QnPolarPoint<float> polarSpeed = cartesianToPolar(cartesianSpeed);
        polarSpeed.alpha = qRound(polarSpeed.alpha, M_PI / 4.0); /* Rounded to 45 degrees. */
        cartesianSpeed = polarToCartesian<QVector2D>(polarSpeed.r, polarSpeed.alpha);

        if(qFuzzyIsNull(cartesianSpeed.x())) // TODO: #Elric use lower null threshold
            cartesianSpeed.setX(0.0);
        if(qFuzzyIsNull(cartesianSpeed.y()))
            cartesianSpeed.setY(0.0);
        
        return base_type::continuousMove(QVector3D(cartesianSpeed, speed.z()));
    }
}
