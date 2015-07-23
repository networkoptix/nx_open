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
        m_capabilities |= Qn::ContinuousPanCapability;
    if( ptzCapabilities & nxcip::CameraPtzManager::ContinuousTiltCapability )
        m_capabilities |= Qn::ContinuousTiltCapability;
    if( ptzCapabilities & nxcip::CameraPtzManager::ContinuousZoomCapability )
        m_capabilities |= Qn::ContinuousZoomCapability;
    if( ptzCapabilities & nxcip::CameraPtzManager::AbsolutePanCapability )
        m_capabilities |= Qn::AbsolutePanCapability;
    if( ptzCapabilities & nxcip::CameraPtzManager::AbsoluteTiltCapability )
        m_capabilities |= Qn::AbsoluteTiltCapability;
    if( ptzCapabilities & nxcip::CameraPtzManager::AbsoluteZoomCapability )
        m_capabilities |= Qn::AbsoluteZoomCapability;
    if( ptzCapabilities & nxcip::CameraPtzManager::FlipPtzCapability )
        m_capabilities |= Qn::FlipPtzCapability;
    if( ptzCapabilities & nxcip::CameraPtzManager::LimitsPtzCapability )
        m_capabilities |= Qn::LimitsPtzCapability;
    if( ptzCapabilities & nxcip::CameraPtzManager::DevicePositioningPtzCapability )
        m_capabilities |= Qn::DevicePositioningPtzCapability;
    if( ptzCapabilities & nxcip::CameraPtzManager::LogicalPositioningPtzCapability )
        m_capabilities |= Qn::LogicalPositioningPtzCapability;
}

QnThirdPartyPtzController::~QnThirdPartyPtzController()
{
    m_cameraPtzManager->releaseRef();
    m_cameraPtzManager = nullptr;
}

Qn::PtzCapabilities QnThirdPartyPtzController::getCapabilities()
{
    return m_capabilities;
}

bool QnThirdPartyPtzController::continuousMove(const QVector3D &speed)
{
    return m_cameraPtzManager->continuousMove( speed.x(), speed.y(), speed.z() ) == nxcip::NX_NO_ERROR;
}

bool QnThirdPartyPtzController::absoluteMove(Qn::PtzCoordinateSpace space, const QVector3D &position, qreal speed)
{
    return m_cameraPtzManager->absoluteMove(
        space == Qn::DevicePtzCoordinateSpace ? nxcip::CameraPtzManager::DevicePtzCoordinateSpace : nxcip::CameraPtzManager::LogicalPtzCoordinateSpace,
        position.x(), position.y(), position.z(),
        speed ) == nxcip::NX_NO_ERROR;
}

bool QnThirdPartyPtzController::getPosition(Qn::PtzCoordinateSpace space, QVector3D *position)
{
    double pan = 0, tilt = 0, zoom = 0;
    if( m_cameraPtzManager->getPosition(
            space == Qn::DevicePtzCoordinateSpace ? nxcip::CameraPtzManager::DevicePtzCoordinateSpace : nxcip::CameraPtzManager::LogicalPtzCoordinateSpace,
            &pan, &tilt, &zoom ) != nxcip::NX_NO_ERROR )
        return false;

    position->setX( pan );
    position->setY( tilt );
    position->setZ( zoom );
    return true;
}

bool QnThirdPartyPtzController::getLimits(Qn::PtzCoordinateSpace /*space*/, QnPtzLimits* /*limits*/)
{
    //TODO/IMPL
    return false;
}

bool QnThirdPartyPtzController::getFlip(Qt::Orientations* /*flip*/)
{
    //TODO/IMPL
    return false;
}

#endif // ENABLE_THIRD_PARTY

