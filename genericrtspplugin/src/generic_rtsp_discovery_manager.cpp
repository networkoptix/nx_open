/**********************************************************
* 22 apr 2013
* akolesnikov
***********************************************************/

#include "generic_rtsp_discovery_manager.h"

#include <cstring>

#include <QUrl>

#include "generic_rtsp_camera_manager.h"
#include "generic_rtsp_plugin.h"


GenericRTSPDiscoveryManager::GenericRTSPDiscoveryManager()
:
    m_refManager( GenericRTSPPlugin::instance()->refManager() )
{
}

void* GenericRTSPDiscoveryManager::queryInterface( const nxpl::NX_GUID& interfaceID )
{
    if( memcmp( &interfaceID, &nxcip::IID_CameraDiscoveryManager, sizeof(nxcip::IID_CameraDiscoveryManager) ) == 0 )
    {
        addRef();
        return this;
    }
    return NULL;
}

unsigned int GenericRTSPDiscoveryManager::addRef()
{
    return m_refManager.addRef();
}

unsigned int GenericRTSPDiscoveryManager::releaseRef()
{
    return m_refManager.releaseRef();
}

static const char* VENDOR_NAME = "GENERIC_RTSP";

void GenericRTSPDiscoveryManager::getVendorName( char* buf ) const
{
    strcpy( buf, VENDOR_NAME );
}

int GenericRTSPDiscoveryManager::findCameras( nxcip::CameraInfo* cameras, const char* localInterfaceIPAddr )
{
#if 0
    strcpy( cameras[0].url, "rtsp://192.168.0.31:554/axis-media/media.amp" );
    strcpy( cameras[0].uid, "HUY" );
    strcpy( cameras[0].modelName, "rtsp://192.168.0.31:554/axis-media/media.amp" );
    return 1;
#else
    return 0;
#endif
}

int GenericRTSPDiscoveryManager::checkHostAddress( nxcip::CameraInfo* cameras, const char* address, const char* login, const char* password )
{
    QUrl url( address );
    if( url.scheme().toLower() != "rtsp" )
        return 0;

    //TODO/IMPL trying to connect to \a address as RTSP url
        //if audio is present at url, remembering it in CameraInfo

    //cameras[0].modelName[256];
    //cameras[0].firmware[256];
    memset( &cameras[0], 0, sizeof(cameras[0]) );
    strncpy( cameras[0].uid, address, sizeof(cameras[0].uid)-1 );
    strncpy( cameras[0].url, address, sizeof(cameras[0].url)-1 );
    //cameras[0].auxiliaryData[256];
    strncpy( cameras[0].defaultLogin, login, sizeof(cameras[0].defaultLogin)-1 );
    strncpy( cameras[0].defaultPassword, password, sizeof(cameras[0].defaultPassword)-1 );

    return 1;
}

int GenericRTSPDiscoveryManager::fromMDNSData(
    const char* /*discoveredAddress*/,
    const unsigned char* /*mdnsResponsePacket*/,
    int /*mdnsResponsePacketSize*/,
    nxcip::CameraInfo* /*cameraInfo*/ )
{
    return nxcip::NX_NO_ERROR;
}

int GenericRTSPDiscoveryManager::fromUpnpData( const char* /*upnpXMLData*/, int /*upnpXMLDataSize*/, nxcip::CameraInfo* /*cameraInfo*/ )
{
    return nxcip::NX_NO_ERROR;
}

nxcip::BaseCameraManager* GenericRTSPDiscoveryManager::createCameraManager( const nxcip::CameraInfo& info )
{
    const nxcip::CameraInfo& infoCopy( info );
    if( strlen(infoCopy.auxiliaryData) == 0 )
    {
        //TODO/IMPL checking, if audio is present at info.url
    }
    return new GenericRTSPCameraManager( infoCopy );
}

void GenericRTSPDiscoveryManager::getReservedModelListFirst( char** /*modelList*/, int* /*count*/ )
{
}

void GenericRTSPDiscoveryManager::getReservedModelListNext( char** /*modelList*/, int* /*count*/ )
{
}
