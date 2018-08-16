/**********************************************************
* 24 apr 2014
* akolesnikov
***********************************************************/

#include "third_party_ptz_controller.h"

#include <nx/utils/log/log.h>
#include <nx/utils/log/assert.h>

#ifdef ENABLE_THIRD_PARTY

using namespace nx::core;

QnThirdPartyPtzController::QnThirdPartyPtzController(
    const QnThirdPartyResourcePtr& resource,
    nxcip::CameraPtzManager* cameraPtzManager )
:
    QnBasicPtzController( resource ),
    m_resource( resource ),
    m_cameraPtzManager( cameraPtzManager ),
    m_capabilities( 0 ),
    m_flip( 0 )
{
    const int ptzCapabilities = cameraPtzManager->getCapabilities();
    if( ptzCapabilities & nxcip::CameraPtzManager::ContinuousPanCapability )
        m_capabilities |= Ptz::ContinuousPanCapability;
    if( ptzCapabilities & nxcip::CameraPtzManager::ContinuousTiltCapability )
        m_capabilities |= Ptz::ContinuousTiltCapability;
    if( ptzCapabilities & nxcip::CameraPtzManager::ContinuousZoomCapability )
        m_capabilities |= Ptz::ContinuousZoomCapability;
    if( ptzCapabilities & nxcip::CameraPtzManager::AbsolutePanCapability )
        m_capabilities |= Ptz::AbsolutePanCapability;
    if( ptzCapabilities & nxcip::CameraPtzManager::AbsoluteTiltCapability )
        m_capabilities |= Ptz::AbsoluteTiltCapability;
    if( ptzCapabilities & nxcip::CameraPtzManager::AbsoluteZoomCapability )
        m_capabilities |= Ptz::AbsoluteZoomCapability;
    if( ptzCapabilities & nxcip::CameraPtzManager::FlipPtzCapability )
        m_capabilities |= Ptz::FlipPtzCapability;
    if( ptzCapabilities & nxcip::CameraPtzManager::LimitsPtzCapability )
        m_capabilities |= Ptz::LimitsPtzCapability;
    if( ptzCapabilities & nxcip::CameraPtzManager::DevicePositioningPtzCapability )
        m_capabilities |= Ptz::DevicePositioningPtzCapability;
    if( ptzCapabilities & nxcip::CameraPtzManager::LogicalPositioningPtzCapability )
        m_capabilities |= Ptz::LogicalPositioningPtzCapability;
}

QnThirdPartyPtzController::~QnThirdPartyPtzController()
{
    m_cameraPtzManager->releaseRef();
    m_cameraPtzManager = nullptr;
}

Ptz::Capabilities QnThirdPartyPtzController::getCapabilities(
    const nx::core::ptz::Options& options) const
{
    if (options.type != ptz::Type::operational)
        return Ptz::NoPtzCapabilities;

    return m_capabilities;
}

bool QnThirdPartyPtzController::continuousMove(
    const nx::core::ptz::Vector& speedVector,
    const nx::core::ptz::Options& options)
{
    if (options.type != ptz::Type::operational)
    {
        NX_WARNING(
            this,
            lm("Continuous movement - wrong PTZ type. "
                "Only operational PTZ is supported. Resource %1 (%2)")
                .args(resource()->getName(), resource()->getId()));

        return false;
    }

    return m_cameraPtzManager->continuousMove(
        speedVector.pan,
        speedVector.tilt,
        speedVector.zoom) == nxcip::NX_NO_ERROR;
}

bool QnThirdPartyPtzController::absoluteMove(
    Qn::PtzCoordinateSpace space,
    const nx::core::ptz::Vector& position,
    qreal speed,
    const nx::core::ptz::Options& options)
{
    if (options.type != ptz::Type::operational)
    {
        NX_WARNING(
            this,
            lm("Absolute movement - wrong PTZ type. "
                "Only operational PTZ is supported. Resource %1 (%2)")
                .args(resource()->getName(), resource()->getId()));

        return false;
    }

    return m_cameraPtzManager->absoluteMove(
        space == Qn::DevicePtzCoordinateSpace
            ? nxcip::CameraPtzManager::DevicePtzCoordinateSpace
            : nxcip::CameraPtzManager::LogicalPtzCoordinateSpace,
        position.pan,
        position.tilt,
        position.zoom,
        speed) == nxcip::NX_NO_ERROR;
}

bool QnThirdPartyPtzController::getPosition(
    Qn::PtzCoordinateSpace space,
    nx::core::ptz::Vector* outPosition,
    const nx::core::ptz::Options& options) const
{
    if (options.type != ptz::Type::operational)
    {
        NX_WARNING(
            this,
            lm("Getting current position - wrong PTZ type. "
                "Only operational PTZ is supported. Resource %1 (%2)")
                .args(resource()->getName(), resource()->getId()));

        return false;
    }

    return m_cameraPtzManager->getPosition(
        space == Qn::DevicePtzCoordinateSpace
            ? nxcip::CameraPtzManager::DevicePtzCoordinateSpace
            : nxcip::CameraPtzManager::LogicalPtzCoordinateSpace,
        &outPosition->pan,
        &outPosition->tilt,
        &outPosition->zoom) == nxcip::NX_NO_ERROR;
}

bool QnThirdPartyPtzController::getLimits(
    Qn::PtzCoordinateSpace /*space*/,
    QnPtzLimits* /*limits*/,
    const nx::core::ptz::Options& /*options*/) const
{
    //TODO/IMPL
    return false;
}

bool QnThirdPartyPtzController::getFlip(
    Qt::Orientations* /*flip*/,
    const nx::core::ptz::Options& /*options*/) const
{
    //TODO/IMPL
    return false;
}

#endif // ENABLE_THIRD_PARTY

