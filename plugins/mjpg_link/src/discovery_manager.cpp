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

#include <QtCore/QCryptographicHash>
#include <QtCore/QUrl>

#include "camera_manager.h"
#include "plugin.h"


DiscoveryManager::DiscoveryManager()
:
    m_refManager( HttpLinkPlugin::instance()->refManager() )
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

static const char* VENDOR_NAME = "HTTP_URL_PLUGIN";

void DiscoveryManager::getVendorName( char* buf ) const
{
    strcpy( buf, VENDOR_NAME );
}

int DiscoveryManager::findCameras( nxcip::CameraInfo* /*cameras*/, const char* /*localInterfaceIPAddr*/ )
{
    return nxcip::NX_NOT_IMPLEMENTED;
}

static const QString HTTP_PROTO_NAME( QString::fromLatin1("http") );
static const QString HTTPS_PROTO_NAME( QString::fromLatin1("https") );

int DiscoveryManager::checkHostAddress( nxcip::CameraInfo* cameras, const char* address, const char* login, const char* password )
{
    QUrl url( QString::fromUtf8(address) );
    if( url.scheme() != HTTP_PROTO_NAME && url.scheme() != HTTPS_PROTO_NAME )
        return 0;

    //TODO/IMPL checking content type

    const QByteArray& uidStr = QCryptographicHash::hash( QByteArray::fromRawData(address, strlen(address)), QCryptographicHash::Md5 ).toHex();

    memset( &cameras[0], 0, sizeof(cameras[0]) );
    strncpy( cameras[0].uid, uidStr.constData(), sizeof(cameras[0].uid)-1 );
    strncpy( cameras[0].url, address, sizeof(cameras[0].url)-1 );
    //cameras[0].auxiliaryData[256];
    if( login )
        strncpy( cameras[0].defaultLogin, login, sizeof(cameras[0].defaultLogin)-1 );
    if( password )
        strncpy( cameras[0].defaultPassword, password, sizeof(cameras[0].defaultPassword)-1 );

    return 1;
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
