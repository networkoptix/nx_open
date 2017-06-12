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

bool QnThirdPartyPtzController::getPosition(Qn::PtzCoordinateSpace space, QVector3D *position) const
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

