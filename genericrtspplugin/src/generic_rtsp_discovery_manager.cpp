/**********************************************************
* 22 apr 2013
* akolesnikov
***********************************************************/

#include "generic_rtsp_discovery_manager.h"

#include <cstring>

#include "generic_rtsp_camera_manager.h"
#include "generic_rtsp_plugin.h"


GenericRTSPDiscoveryManager::GenericRTSPDiscoveryManager()
:
    CommonRefManager( GenericRTSPPlugin::instance() )
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

static const char* VENDOR_NAME = "GENERIC_RTSP";

void GenericRTSPDiscoveryManager::getVendorName( char* buf ) const
{
    strcpy( buf, VENDOR_NAME );
}

int GenericRTSPDiscoveryManager::findCameras( nxcip::CameraInfo* cameras, const char* localInterfaceIPAddr )
{
    strcpy( cameras[0].url, "rtsp://192.168.0.31:554/axis-media/media.amp" );
    strcpy( cameras[0].uid, "HUY" );
    strcpy( cameras[0].modelName, "rtsp://192.168.0.31:554/axis-media/media.amp" );
    return 1;
}

int GenericRTSPDiscoveryManager::checkHostAddress( nxcip::CameraInfo* cameras, const char* address, const char* login, const char* password )
{
    //TODO/IMPL trying to connect to \a address as RTSP url
        //if audio is present at url, remembering it in CameraInfo
    return nxcip::NX_NO_ERROR;
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
