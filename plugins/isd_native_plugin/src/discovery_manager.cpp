/**********************************************************
* 03 sep 2013
* akolesnikov
***********************************************************/

#include "discovery_manager.h"

#include <sys/types.h>
#include <sys/stat.h>
#ifdef _POSIX_C_SOURCE
#include <unistd.h>
#endif
#include <cstdio>

#include "camera_manager.h"
#include "plugin.h"


DiscoveryManager::DiscoveryManager()
:
    m_refManager( IsdNativePlugin::instance()->refManager() )
{
}

void* DiscoveryManager::queryInterface( const nxpl::NX_GUID& interfaceID )
{
    if( memcmp( &interfaceID, &nxcip::IID_CameraDiscoveryManager, sizeof(nxcip::IID_CameraDiscoveryManager) ) == 0 )
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

unsigned int DiscoveryManager::addRef()
{
    return m_refManager.addRef();
}

unsigned int DiscoveryManager::releaseRef()
{
    return m_refManager.releaseRef();
}

static const char* VENDOR_NAME = "ISD_NATIVE";

void DiscoveryManager::getVendorName( char* buf ) const
{
    strcpy( buf, VENDOR_NAME );
}

int DiscoveryManager::findCameras( nxcip::CameraInfo* /*cameras*/, const char* /*localInterfaceIPAddr*/ )
{
    return nxcip::NX_NOT_IMPLEMENTED;
}

int DiscoveryManager::checkHostAddress( nxcip::CameraInfo* cameras, const char* address, const char* /*login*/, const char* /*password*/ )
{
    return nxcip::NX_NO_ERROR;
}

int DiscoveryManager::fromMDNSData(
    const char* /*discoveredAddress*/,
    const unsigned char* /*mdnsResponsePacket*/,
    int /*mdnsResponsePacketSize*/,
    nxcip::CameraInfo* /*cameraInfo*/ )
{
    return nxcip::NX_NO_ERROR;
}

int DiscoveryManager::fromUpnpData( const char* /*upnpXMLData*/, int /*upnpXMLDataSize*/, nxcip::CameraInfo* /*cameraInfo*/ )
{
    return nxcip::NX_NO_ERROR;
}

nxcip::BaseCameraManager* DiscoveryManager::createCameraManager( const nxcip::CameraInfo& info )
{
    const nxcip::CameraInfo& infoCopy( info );
    if( strlen(infoCopy.auxiliaryData) == 0 )
    {
        //TODO/IMPL checking, if audio is present at info.url
    }
    return new CameraManager( infoCopy );
}

int DiscoveryManager::getReservedModelList( char** /*modelList*/, int* count )
{
    *count = 0;
    return nxcip::NX_NO_ERROR;
}
