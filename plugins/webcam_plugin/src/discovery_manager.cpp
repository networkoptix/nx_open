/**********************************************************
* 03 sep 2013
* akolesnikov
***********************************************************/

//#pragma comment(lib, "strmiids.lib")

#include "discovery_manager.h"

#include <sys/types.h>
#include <sys/stat.h>
#ifdef _POSIX_C_SOURCE
#include <unistd.h>
#endif
#include <cstdio>

#include <QtCore/QCryptographicHash>
#include <nx/network/http/http_client.h>
#include <nx/network/http/multipart_content_parser.h>

#include "utils.h"
#include "camera_manager.h"
#include "plugin.h"

namespace {

int findDShowCameras(nxcip::CameraInfo * cameras, const char * localIpInterfaceIpAddr)
{
    QList<utils::DeviceInfo> devices = utils::getDeviceList();
    int deviceCount = devices.count();
    for (int i = 0; i < deviceCount && i < nxcip::CAMERA_INFO_ARRAY_SIZE; ++i)
    {
        QString deviceName = devices[i].deviceName();
        strcpy(cameras[i].modelName, deviceName.toLatin1().data());

        QByteArray url =
            QByteArray("webcam://").append(nx::utils::Url::toPercentEncoding(devices[i].devicePath()));
        strcpy(cameras[i].url, url.data());

        const QByteArray& uid = QCryptographicHash::hash(url, QCryptographicHash::Md5).toHex();
        strcpy(cameras[i].uid, uid.data());

    }
    return deviceCount;
}
int findV4L2Cameras(nxcip::CameraInfo * cameras, const char * localIpInterfaceIpAddr)
{
    return 0; // todo
}

int findAvFoundationCameras(nxcip::CameraInfo * cameras, const char * localIpInterfaceIpAddr)
{
    return 0; // todo
}

} // namespace

DiscoveryManager::DiscoveryManager(nxpt::CommonRefManager* const refManager, 
                                   nxpl::TimeProvider *const timeProvider)
:
    m_refManager( refManager ),
    m_timeProvider( timeProvider )
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

static const char* VENDOR_NAME = "WEBCAM_PLUGIN";

void DiscoveryManager::getVendorName(char* buf) const
{
    strcpy(buf, VENDOR_NAME);
}

int DiscoveryManager::findCameras( nxcip::CameraInfo* cameras, const char* localInterfaceIPAddr )
{    
    QList<utils::DeviceInfo> devices = utils::getDeviceList();
    int deviceCount = devices.count();
    for (int i = 0; i < deviceCount && i < nxcip::CAMERA_INFO_ARRAY_SIZE; ++i)
    {
        QString deviceName = devices[i].deviceName();
        strcpy(cameras[i].modelName, deviceName.toLatin1().data());

        QByteArray url =
            QByteArray("webcam://").append(
                nx::utils::Url::toPercentEncoding(devices[i].devicePath()));
        strcpy(cameras[i].url, url.data());

        const QByteArray& uid = QCryptographicHash::hash(url, QCryptographicHash::Md5).toHex();
        strcpy(cameras[i].uid, uid.data());

    }
    return deviceCount;
}

//static const QString HTTP_PROTO_NAME( QString::fromLatin1("http") );
//static const QString HTTPS_PROTO_NAME( QString::fromLatin1("https") );

int DiscoveryManager::checkHostAddress( nxcip::CameraInfo* cameras, const char* address, const char* login, const char* password )
{
    //host address doesn't mean anything for a local web cam
    return 0;
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
    return new CameraManager(info, m_timeProvider);
}

int DiscoveryManager::getReservedModelList( char** /*modelList*/, int* count )
{
    *count = 0;
    return nxcip::NX_NO_ERROR;
}

