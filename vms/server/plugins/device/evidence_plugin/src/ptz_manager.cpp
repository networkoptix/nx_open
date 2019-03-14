/**********************************************************
* 23 apr 2014
* akolesnikov
***********************************************************/

#include "ptz_manager.h"

#include <iostream>

#include "camera_manager.h"
#include "camera_plugin.h"
#include "sync_http_client.h"


PtzManager::PtzManager(
    CameraManager* cameraManager,
    const QList<QByteArray>& cameraParamList,
    const QList<QByteArray>& cameraParamOptions )
:
    m_refManager( cameraManager->refManager() ),
    m_cameraManager( cameraManager ),
    m_capabilities( 0 ),
    m_flip( 0 )
{
    readParameters( cameraParamList, cameraParamOptions );
}

void* PtzManager::queryInterface( const nxpl::NX_GUID& interfaceID )
{
    if( memcmp( &interfaceID, &nxcip::IID_BaseCameraManager, sizeof(nxcip::IID_BaseCameraManager) ) == 0 )
    {
        addRef();
        return this;
    }
    if( memcmp( &interfaceID, &nxpl::IID_PluginInterface, sizeof(nxpl::IID_PluginInterface) ) == 0 )
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return NULL;
}

int PtzManager::addRef() const
{
    return m_refManager.addRef();
}

int PtzManager::releaseRef() const
{
    return m_refManager.releaseRef();
}

int PtzManager::getCapabilities() const
{
    return m_capabilities;
}

int PtzManager::continuousMove( double panSpeed, double tiltSpeed, double zoomSpeed )
{
    SyncHttpClient httpClient(
        CameraPlugin::instance()->networkAccessManager(),
        m_cameraManager->cameraInfo().url,
        m_cameraManager->apiPort(),
        m_cameraManager->credentials() );

    const int result = CameraManager::doCameraRequest(
        &httpClient,
        QString::fromLatin1("com/ptz.cgi?continuouspantiltmove=%1,%2&continuouszoommove=%3").arg(panSpeed * 100).arg(tiltSpeed * 100).arg(zoomSpeed * 100).toLatin1() );
    if( result != nxcip::NX_NO_ERROR )
        return result;
    if( !(panSpeed == 0.0 && tiltSpeed == 0.0 && zoomSpeed == 0.0) )
        return result;
    //sending second "stop" request just for sure, since camera does not always stop
    return CameraManager::doCameraRequest(
        &httpClient,
        QString::fromLatin1("com/ptz.cgi?continuouspantiltmove=%1,%2&continuouszoommove=%3").arg(0).arg(0).arg(0).toLatin1() );
}

int PtzManager::absoluteMove( CoordinateSpace space, double pan, double tilt, double zoom, double speed )
{
    if( space != nxcip::CameraPtzManager::DevicePtzCoordinateSpace )
        return nxcip::NX_NOT_IMPLEMENTED;

    SyncHttpClient httpClient(
        CameraPlugin::instance()->networkAccessManager(),
        m_cameraManager->cameraInfo().url,
        m_cameraManager->apiPort(),
        m_cameraManager->credentials() );
    return CameraManager::doCameraRequest(
        &httpClient,
        QString::fromLatin1("com/ptz.cgi?pan=%1&tilt=%2&zoom=%3&speed=%4").arg(pan).arg(tilt).arg(toCameraZoomUnits(zoom)).arg(speed * 100).toLatin1() );
}

int PtzManager::getPosition( CoordinateSpace space, double* pan, double* tilt, double* zoom )
{
    if( space != nxcip::CameraPtzManager::DevicePtzCoordinateSpace )
        return nxcip::NX_NOT_IMPLEMENTED;

    SyncHttpClient httpClient(
        CameraPlugin::instance()->networkAccessManager(),
        m_cameraManager->cameraInfo().url,
        m_cameraManager->apiPort(),
        m_cameraManager->credentials() );

    std::multimap<QByteArray, QByteArray> params;
    int result = CameraManager::doCameraRequest( &httpClient, "com/ptz.cgi?query=position", &params );
    if( result != nxcip::NX_NO_ERROR )
        return result;

    auto panIter = params.find( "pan" );
    auto tiltIter = params.find( "tilt" );
    auto zoomIter = params.find( "zoom" );
    if( (panIter != params.end()) && (tiltIter != params.end()) && (zoomIter != params.end()) )
    {
        *pan = panIter->second.toDouble();
        *tilt = tiltIter->second.toDouble();
        *zoom = fromCameraZoomUnits(zoomIter->second.toDouble());
        return nxcip::NX_NO_ERROR;
    }
    else
    {
        return nxcip::NX_OTHER_ERROR;
    }
}

int PtzManager::getLimits( CoordinateSpace space, Limits* limits )
{
    if( space != nxcip::CameraPtzManager::DevicePtzCoordinateSpace )
        return nxcip::NX_NOT_IMPLEMENTED;

    *limits = m_limits;
    return nxcip::NX_NO_ERROR;
}

int PtzManager::getFlip( int* flip )
{
    //*flip = m_flip;
    //return nxcip::NX_NO_ERROR;
    return nxcip::NX_NOT_IMPLEMENTED;
}

double PtzManager::toCameraZoomUnits( double val ) const
{
    //TODO/IMPL m_35mmEquivToCameraZoom(qDegreesTo35mmEquiv
    return val;
}

double PtzManager::fromCameraZoomUnits( double val ) const
{
    //TODO/IMPL  q35mmEquivToDegrees(m_cameraTo35mmEquivZoom
    return val;
}

void PtzManager::readParameters(
    const QList<QByteArray>& cameraParamList,
    const QList<QByteArray>& /*cameraParamOptions*/ )
{
    memset( &m_limits, 0, sizeof(m_limits) );
    m_limits.minPan = -180;
    m_limits.maxPan = 180;
    m_limits.minTilt = -180;
    m_limits.maxTilt = 180;
    m_limits.minFov = 0;
    m_limits.maxFov = 9999;

    for( const QByteArray& paramStr: cameraParamList )
    {
        const QList<QByteArray>& paramValueList = paramStr.split( '=' );
        if( paramValueList.size() < 2 )
            continue;

        if( paramValueList[0] == "root.PTZ.Limit.L0.Mintilt" )
            m_limits.minTilt = paramValueList[1].toDouble();
        else if( paramValueList[0] == "root.PTZ.Limit.L0.Maxtilt" )
            m_limits.maxTilt = paramValueList[1].toDouble();
    }

    m_flip = nxcip::CameraPtzManager::Vertical | nxcip::CameraPtzManager::Horizontal;
    m_capabilities |=
        nxcip::CameraPtzManager::ContinuousPtzCapabilities
        | nxcip::CameraPtzManager::AbsolutePtzCapabilities
        | nxcip::CameraPtzManager::DevicePositioningPtzCapability
        | nxcip::CameraPtzManager::DevicePtzCoordinateSpace
        ;
        //| nxcip::CameraPtzManager::LimitsPtzCapability
        //| nxcip::CameraPtzManager::FlipPtzCapability;
}
