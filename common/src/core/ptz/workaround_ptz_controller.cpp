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
        m_overrideCapabilities = true;
        m_capabilities = Qn::NoPtzCapabilities;
        foreach(const QString& capability, ptzCapabilities.split(L'|'))
        {
            if(capability == lit("NoPtzCapabilities"))
                m_capabilities |= Qn::NoPtzCapabilities;
            else if(capability == lit("ContinuousPanCapability"))
                m_capabilities |= Qn::ContinuousPanCapability;
            else if(capability == lit("ContinuousTiltCapability"))
                m_capabilities |= Qn::ContinuousTiltCapability;
            else if(capability == lit("ContinuousPanTiltCapabilities"))
                m_capabilities |= Qn::ContinuousPanTiltCapabilities;
            else if(capability == lit("ContinuousPtzCapabilities"))
                m_capabilities |= Qn::ContinuousPtzCapabilities;
            else if(capability == lit("ContinuousZoomCapability"))
                m_capabilities |= Qn::ContinuousZoomCapability;
            else if(capability == lit("ContinuousFocusCapability"))
                m_capabilities |= Qn::ContinuousFocusCapability;
            else if(capability == lit("AbsolutePanCapability"))
                m_capabilities |= Qn::AbsolutePanCapability;
            else if(capability == lit("AbsoluteTiltCapability"))
                m_capabilities |= Qn::AbsoluteTiltCapability;
            else if(capability == lit("AbsoluteZoomCapability"))
                m_capabilities |= Qn::AbsoluteZoomCapability;
            else if(capability == lit("AbsolutePtzCapabilities"))
                m_capabilities |= Qn::AbsolutePtzCapabilities;
            else if(capability == lit("ViewportPtzCapability"))
                m_capabilities |= Qn::ViewportPtzCapability;
            else if(capability == lit("FlipPtzCapability"))
                m_capabilities |= Qn::FlipPtzCapability;
            else if(capability == lit("LimitsPtzCapability"))
                m_capabilities |= Qn::LimitsPtzCapability;
            else if(capability == lit("LimitsPtzCapability"))
                m_capabilities |= Qn::LimitsPtzCapability;
            else if(capability == lit("DevicePositioningPtzCapability"))
                m_capabilities |= Qn::DevicePositioningPtzCapability;
            else if(capability == lit("LogicalPositioningPtzCapability"))
                m_capabilities |= Qn::LogicalPositioningPtzCapability;
            else if(capability == lit("PresetsPtzCapability"))
                m_capabilities |= Qn::PresetsPtzCapability;
            else if(capability == lit("ToursPtzCapability"))
                m_capabilities |= Qn::ToursPtzCapability;
            else if(capability == lit("ActivityPtzCapability"))
                m_capabilities |= Qn::ActivityPtzCapability;
            else if(capability == lit("HomePtzCapability"))
                m_capabilities |= Qn::HomePtzCapability;
            else if(capability == lit("AsynchronousPtzCapability"))
                m_capabilities |= Qn::AsynchronousPtzCapability;
            else if(capability == lit("SynchronizedPtzCapability"))
                m_capabilities |= Qn::SynchronizedPtzCapability;
            else if(capability == lit("VirtualPtzCapability"))
                m_capabilities |= Qn::VirtualPtzCapability;
            else if(capability == lit("nativePresetsPtzCapability"))
                m_capabilities |= Qn::nativePresetsPtzCapability;
            else
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

