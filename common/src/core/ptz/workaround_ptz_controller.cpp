#include "workaround_ptz_controller.h"

#include <utils/math/math.h>
#include <utils/math/coordinate_transformations.h>
#include <utils/common/lexical.h>

#include <common/common_module.h>
#include <core/resource/resource_data.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_data_pool.h>

QnWorkaroundPtzController::QnWorkaroundPtzController(const QnPtzControllerPtr &baseController):
    base_type(baseController),
    m_overrideContinuousMove(false),
    m_flip(0),
    m_trait(Qn::NoPtzTraits),
    m_overrideCapabilities(false),
    m_capabilities(Qn::NoPtzCapabilities)
{
    QnVirtualCameraResourcePtr camera = resource().dynamicCast<QnVirtualCameraResource>();
    if(!camera)
        return;

    QnResourceData resourceData = qnCommon->dataPool()->data(camera, true);
    
    QString ptzTraits = resourceData.value<QString>(lit("ptzTraits"));
    if(!ptzTraits.isEmpty())
        if(!QnLexical::deserialize(ptzTraits, &m_trait)) // TODO: #Elric support flags in 2.3
            qnWarning("Could not parse PTZ traits '%1'.", ptzTraits);

    if(resourceData.value<bool>(lit("panFlipped"), false))
        m_flip |= Qt::Horizontal;
    if(resourceData.value<bool>(lit("tiltFlipped"), false))
        m_flip |= Qt::Vertical;

    m_overrideContinuousMove = m_flip != 0 || (m_trait & (Qn::FourWayPtzTrait | Qn::EightWayPtzTrait));

    QString ptzCapabilities = resourceData.value<QString>(lit("ptzCapabilities"));
    if(!ptzCapabilities.isEmpty()) {
        // TODO: #Elric evil. implement via enum name mapper flags.

        if(ptzCapabilities == lit("NoPtzCapabilities")) {
            m_overrideCapabilities = true;
            m_capabilities = Qn::NoPtzCapabilities;
        } else if(ptzCapabilities == lit("ContinuousPanCapability")) {
            m_overrideCapabilities = true;
            m_capabilities = Qn::ContinuousPanCapability;
        } else if(ptzCapabilities == lit("ContinuousZoomCapability")) {
            m_overrideCapabilities = true;
            m_capabilities = Qn::ContinuousZoomCapability;
        } else if(ptzCapabilities == lit("ContinuousPanCapability|ContinuousTiltCapability")) {
            m_overrideCapabilities = true;
            m_capabilities = Qn::ContinuousPanCapability | Qn::ContinuousTiltCapability;
        } else if(ptzCapabilities == lit("ContinuousPanCapability|ContinuousTiltCapability|ContinuousZoomCapability")) {
            m_overrideCapabilities = true;
            m_capabilities = Qn::ContinuousPanCapability | Qn::ContinuousTiltCapability | Qn::ContinuousZoomCapability;
        } else if(ptzCapabilities == lit("ContinuousPanCapability|ContinuousTiltCapability|ContinuousZoomCapability|ContinuousFocusCapability")) {
            m_overrideCapabilities = true;
            m_capabilities = Qn::ContinuousPanCapability | Qn::ContinuousTiltCapability | Qn::ContinuousZoomCapability | Qn::ContinuousFocusCapability;
        } else if(ptzCapabilities == lit("ContinuousPanCapability|ContinuousTiltCapability|AbsolutePanCapability|AbsoluteTiltCapability|DevicePositioningPtzCapability")) {
            m_overrideCapabilities = true;
            m_capabilities = Qn::ContinuousPanCapability | Qn::ContinuousTiltCapability | Qn::AbsolutePanCapability | Qn::AbsoluteTiltCapability | Qn::DevicePositioningPtzCapability;
        } else if(ptzCapabilities == lit("ContinuousPanCapability|ContinuousTiltCapability|ContinuousZoomCapability|ContinuousFocusCapability|AuxilaryPtzCapability")) {
            m_overrideCapabilities = true;
            m_capabilities = Qn::ContinuousPanCapability|Qn::ContinuousTiltCapability|Qn::ContinuousZoomCapability|Qn::ContinuousFocusCapability|Qn::AuxilaryPtzCapability;
        } else if(ptzCapabilities == lit("ContinuousZoomCapability|ContinuousFocusCapability|AuxilaryPtzCapability")) {
            m_overrideCapabilities = true;
            m_capabilities = Qn::ContinuousZoomCapability|Qn::ContinuousFocusCapability|Qn::AuxilaryPtzCapability;
        } else {
            qnWarning("Could not parse PTZ capabilities '%1'.", ptzCapabilities);
        }
    }
}

Qn::PtzCapabilities QnWorkaroundPtzController::getCapabilities() {
    return m_overrideCapabilities ? m_capabilities : base_type::getCapabilities();
}

bool QnWorkaroundPtzController::continuousMove(const QVector3D &speed) {
    if(!m_overrideContinuousMove)
        return base_type::continuousMove(speed);

    QVector3D localSpeed = speed;
    if(m_flip & Qt::Horizontal)
        localSpeed.setX(localSpeed.x() * -1);
    if(m_flip & Qt::Vertical)
        localSpeed.setY(localSpeed.y() * -1);

    if(m_trait & (Qn::EightWayPtzTrait | Qn::FourWayPtzTrait)) {
        float rounding = (m_trait & Qn::EightWayPtzTrait) ? M_PI / 4.0 : M_PI / 2.0; /* 45 or 90 degrees. */

        QVector2D cartesianSpeed(localSpeed);
        QnPolarPoint<float> polarSpeed = cartesianToPolar(cartesianSpeed);
        polarSpeed.alpha = qRound(polarSpeed.alpha, rounding);
        cartesianSpeed = polarToCartesian<QVector2D>(polarSpeed.r, polarSpeed.alpha);

        if(qFuzzyIsNull(cartesianSpeed.x())) // TODO: #Elric use lower null threshold
            cartesianSpeed.setX(0.0);
        if(qFuzzyIsNull(cartesianSpeed.y()))
            cartesianSpeed.setY(0.0);

        localSpeed = QVector3D(cartesianSpeed, localSpeed.z());
    }

    return base_type::continuousMove(localSpeed);
}

bool QnWorkaroundPtzController::extends(Qn::PtzCapabilities) {
    return true; // TODO: #Elric if no workaround is needed for a camera, we don't really have to extend.
}

