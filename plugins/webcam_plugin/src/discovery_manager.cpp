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
#ifdef _WIN32
#include "dshow.h"
#elif __linux__
#elif __APPLE__
#endif

#include <QtCore/QCryptographicHash>
#include <nx/network/http/http_client.h>
#include <nx/network/http/multipart_content_parser.h>

#include "camera_manager.h"
#include "plugin.h"

namespace {

#ifdef _WIN32
    /**
    * shamelessly borrowed from: https://ffmpeg.zeranoe.com/forum/viewtopic.php?t=651
    */
    QList<QString> dshowListDevices()
    {
        QList<QString> deviceNames;
        ICreateDevEnum *pDevEnum;
        HRESULT hr = CoCreateInstance(CLSID_SystemDeviceEnum, 0, CLSCTX_INPROC_SERVER, IID_ICreateDevEnum, (void**) &pDevEnum);
        if (FAILED(hr))
            return deviceNames;

        IEnumMoniker *pEnum;
        hr = pDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnum, 0);
        if (FAILED(hr))
            return deviceNames;

        IMoniker *pMoniker = NULL;
        while (S_OK == pEnum->Next(1, &pMoniker, NULL))
        {
            IPropertyBag *pPropBag;
            LPOLESTR str = 0;
            hr = pMoniker->GetDisplayName(0, 0, &str);
            if (SUCCEEDED(hr))
            {
                hr = pMoniker->BindToStorage(0, 0, IID_IPropertyBag, (void**) &pPropBag);
                if (SUCCEEDED(hr))
                {
                    VARIANT var;
                    VariantInit(&var);
                    hr = pPropBag->Read(L"FriendlyName", &var, 0);
                    QString fName = QString::fromWCharArray(var.bstrVal);
                    deviceNames.append(fName);
                }
            }
        }
        return deviceNames;
    }
#endif

int dummyFindCameras(nxcip::CameraInfo* cameras, const char* /*localInterfaceIPAddr*/)
{
    int cameraCount = 1;

    const char* dummyModel = "HD Pro Webcam C920";
    strcpy(cameras[0].modelName, dummyModel);
    QByteArray url = nx::utils::Url::toPercentEncoding(dummyModel);
    url.prepend("webcam://");
    const QByteArray& uid = QCryptographicHash::hash(url, QCryptographicHash::Md5 ).toHex();
    strcpy(cameras[0].uid, uid.data());
    strcpy(cameras[0].url, url.data());
    
    return cameraCount;
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
#ifdef _WIN32
        return findDirectShowCameras(cameras, localInterfaceIPAddr);
#elif __linux__
        findVideo4Linux2Cameras(cameras, localInterfaceIPAddr);
#elif __APPLE__
        findAvFoundationCameras(cameras, localInterfaceIPAddr);
#else
        0 // unsuported os
#endif
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

int DiscoveryManager::findDirectShowCameras(nxcip::CameraInfo * cameras, const char * localIpInterfaceIpAddr)
{
    QList<QString> deviceNames = dshowListDevices();
    int deviceCount = deviceNames.count();
    for (int i = 0; i < deviceCount && i < nxcip::CAMERA_INFO_ARRAY_SIZE; ++i)
    {
        strcpy(cameras[i].modelName, deviceNames[i].toLatin1().data());

        QByteArray url = 
            QByteArray("webcam://").append(nx::utils::Url::toPercentEncoding(deviceNames[i]));
        strcpy(cameras[i].url, url.data());

        const QByteArray& uid = QCryptographicHash::hash(url, QCryptographicHash::Md5).toHex();
        strcpy(cameras[i].uid, uid.data());
    }
    return deviceCount;
}

int DiscoveryManager::findVideo4Linux2Cameras(nxcip::CameraInfo * cameras, const char * localIpInterfaceIpAddr)
{
    return 0; // todo
}

int DiscoveryManager::findAvFoundationCameras(nxcip::CameraInfo * cameras, const char * localIpInterfaceIpAddr)
{
    return 0; // todo
}

