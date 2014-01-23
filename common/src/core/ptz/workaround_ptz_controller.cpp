#include "workaround_ptz_controller.h"

#include <utils/math/math.h>
#include <utils/math/coordinate_transformations.h>

#include <common/common_module.h>
#include <core/resource/resource_data.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_data_pool.h>

QnWorkaroundPtzController::QnWorkaroundPtzController(const QnPtzControllerPtr &baseController):
    base_type(baseController),
    m_octagonal(false),
    m_broken(false)
{
    QnVirtualCameraResourcePtr camera = resource().dynamicCast<QnVirtualCameraResource>();
    if(!camera)
        return;

    QnResourceData resourceData = qnCommon->dataPool()->data(camera);

    m_octagonal = resourceData.value<bool>(lit("octagonalPtz"), false);
    m_broken = resourceData.value<bool>(lit("brokenPtz"), false);
}

Qn::PtzCapabilities QnWorkaroundPtzController::getCapabilities() {
    return m_broken ? Qn::NoPtzCapabilities : base_type::getCapabilities();
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

bool QnWorkaroundPtzController::extends(Qn::PtzCapabilities) {
    return true; // TODO: #Elric if no workaround is needed for a camera, we don't really have to extend.
}

