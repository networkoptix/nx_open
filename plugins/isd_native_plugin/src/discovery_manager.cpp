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

#ifndef WIN32
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <net/if.h>
#endif

#include <stdio.h>
#include <string.h>

#include <QtCore/QFile>
#include <utils/network/nettools.h>


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

static const char* VENDOR_NAME = "ISD_EDGE";

void DiscoveryManager::getVendorName( char* buf ) const
{
    strcpy( buf, VENDOR_NAME );
}

int DiscoveryManager::findCameras( nxcip::CameraInfo* cameras, const char* /*localInterfaceIPAddr*/ )
{
    //const char* mac = "543959ab129a";
    char  mac[MAC_ADDR_LEN];
    char* host = 0;
    getMacFromPrimaryIF(mac, &host);

    if( m_modelName.isEmpty() )
    {
        m_modelName = "ISD-xxx";
        QFile hwconfigFile( "/etc/isd/factory/hwconfig.txt" );
        if( hwconfigFile.open( QIODevice::ReadOnly ) )
        {
            const QByteArray& isdParamsStr = hwconfigFile.readAll();
            const QList<QByteArray>& isdParams = isdParamsStr.split('\n');
            for( QByteArray isdParamStr: isdParams )
            {
                const QList<QByteArray>& paramTokens = isdParamStr.split( '=' );
                if( paramTokens.size() < 2 )
                    continue;
                if( paramTokens[0] == "model" )
                {
                    m_modelName = paramTokens[1];
                    break;
                }
            }
        }
    }

    if( m_firmwareVersion.isEmpty() )
    {
        m_firmwareVersion = "unknown";
        QFile versionFile( "/webSvr/web/version.html" );
        if( versionFile.open( QIODevice::ReadOnly ) )
        {
            const QByteArray& isdParamsStr = versionFile.readAll();
            const QList<QByteArray>& isdParams = isdParamsStr.split('\n');
            for( QByteArray isdParamStr: isdParams )
            {
                if( !isdParamStr.startsWith( "Version:" ) )
                    continue;

                const QList<QByteArray>& paramTokens = isdParamStr.split( ':' );
                if( paramTokens.size() < 2 )
                    continue;
                m_firmwareVersion = paramTokens[1].trimmed();
                break;
            }
        }
    }

    const char* loginToUse = "root";
    const char* passwordToUse = "admin";

    memset( cameras, 0, sizeof(*cameras) );
    strncpy( cameras->uid, mac, sizeof(cameras->uid)-1 );
    strncpy( cameras->modelName, m_modelName.constData(), sizeof(cameras->modelName)-1 );
    strncpy( cameras->firmware, m_firmwareVersion.constData(), sizeof(cameras->firmware)-1 );
    if (host)
        strncpy( cameras->url, host, sizeof(cameras->url)-1 );
    strcpy( cameras->defaultLogin, loginToUse );
    strcpy( cameras->defaultPassword, passwordToUse );

    return 1;
}

int DiscoveryManager::checkHostAddress( nxcip::CameraInfo* /*cameras*/, const char* /*address*/, const char* /*login*/, const char* /*password*/ )
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
