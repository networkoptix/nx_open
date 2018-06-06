/**********************************************************
* 24 apr 2014
* akolesnikov
***********************************************************/

#include "third_party_ptz_controller.h"

#ifdef ENABLE_THIRD_PARTY

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

Ptz::Capabilities QnThirdPartyPtzController::getCapabilities() const
{
    return m_capabilities;
}

bool QnThirdPartyPtzController::continuousMove(const nx::core::ptz::Vector& speedVector)
{
    return m_cameraPtzManager->continuousMove(
        speedVector.pan,
        speedVector.tilt,
        speedVector.zoom) == nxcip::NX_NO_ERROR;
}

bool QnThirdPartyPtzController::absoluteMove(
    Qn::PtzCoordinateSpace space,
    const nx::core::ptz::Vector& position,
    qreal speed)
{
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
    nx::core::ptz::Vector* outPosition) const
{
    return m_cameraPtzManager->getPosition(
        space == Qn::DevicePtzCoordinateSpace
            ? nxcip::CameraPtzManager::DevicePtzCoordinateSpace
            : nxcip::CameraPtzManager::LogicalPtzCoordinateSpace,
        &outPosition->pan,
        &outPosition->tilt,
        &outPosition->zoom) == nxcip::NX_NO_ERROR;
}

bool QnThirdPartyPtzController::getLimits(Qn::PtzCoordinateSpace /*space*/, QnPtzLimits* /*limits*/) const
{
    //TODO/IMPL
    return false;
}

bool QnThirdPartyPtzController::getFlip(Qt::Orientations* /*flip*/) const
{
    //TODO/IMPL
    return false;
}

#endif // ENABLE_THIRD_PARTY

