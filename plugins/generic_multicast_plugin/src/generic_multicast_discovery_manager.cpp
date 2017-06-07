#include <atomic>
#include <cstring>

#include <QtCore/QCryptographicHash>
#include <QtCore/QUrl>
#include <QtNetwork/QHostAddress>

#include "generic_multicast_discovery_manager.h"
#include "generic_multicast_camera_manager.h"
#include "generic_multicast_plugin.h"

namespace {

static const char* VENDOR_NAME = "GENERIC_MULTICAST";

} // namespace 


GenericMulticastDiscoveryManager::GenericMulticastDiscoveryManager()
:
    m_refManager(GenericMulticastPlugin::instance()->refManager())
{
}

void* GenericMulticastDiscoveryManager::queryInterface(const nxpl::NX_GUID& interfaceID)
{
    if (memcmp( &interfaceID, &nxcip::IID_CameraDiscoveryManager, sizeof(nxcip::IID_CameraDiscoveryManager)) == 0)
    {
        addRef();
        return static_cast<nxcip::CameraDiscoveryManager*>(this);
    }
    if (memcmp(&interfaceID, &nxpl::IID_PluginInterface, sizeof(nxpl::IID_PluginInterface) ) == 0)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }

    return nullptr;
}

unsigned int GenericMulticastDiscoveryManager::addRef()
{
    return m_refManager.addRef();
}

unsigned int GenericMulticastDiscoveryManager::releaseRef()
{
    return m_refManager.releaseRef();
}

void GenericMulticastDiscoveryManager::getVendorName(char* buf) const
{
    strcpy(buf, VENDOR_NAME);
}

int GenericMulticastDiscoveryManager::findCameras(nxcip::CameraInfo* /*cameras*/, const char* /*localInterfaceIPAddr*/)
{
    return 0;
}

int GenericMulticastDiscoveryManager::checkHostAddress(
    nxcip::CameraInfo* cameras,
    const char* address,
    const char* login,
    const char* password)
{
    QUrl url(address);
    if (url.scheme().toLower() != "udp")
        return 0;

    if (!QHostAddress(url.host()).isMulticast())
        return 0;

    //providing camera uid as md5 hash of url
    const QByteArray& uidStr = QCryptographicHash::hash(
        QByteArray::fromRawData(address, strlen(address)),
        QCryptographicHash::Md5).toHex();

    memset(&cameras[0], 0, sizeof(cameras[0]));
    strncpy(cameras[0].uid, uidStr.constData(), sizeof(cameras[0].uid)-1);
    strncpy(cameras[0].url, address, sizeof(cameras[0].url)-1);
    strncpy(cameras[0].defaultLogin, login, sizeof(cameras[0].defaultLogin)-1);
    strncpy(cameras[0].defaultPassword, password, sizeof(cameras[0].defaultPassword)-1);

    QString modelname = url.fileName();
    if (modelname.isEmpty())    //should not occur, security check
        modelname = url.toString();

    strncpy(cameras[0].modelName, modelname.toUtf8().data(), sizeof(cameras[0].modelName)-1);
    strcpy(cameras[0].auxiliaryData, "generic_multicast_plugin_aux");

    return 1;
}

int GenericMulticastDiscoveryManager::fromMDNSData(
    const char* /*discoveredAddress*/,
    const unsigned char* /*mdnsResponsePacket*/,
    int /*mdnsResponsePacketSize*/,
    nxcip::CameraInfo* /*cameraInfo*/ )
{
    return nxcip::NX_NO_ERROR;
}

int GenericMulticastDiscoveryManager::fromUpnpData(
    const char* /*upnpXMLData*/,
    int /*upnpXMLDataSize*/,
    nxcip::CameraInfo* /*cameraInfo*/ )
{
    return nxcip::NX_NO_ERROR;
}

nxcip::BaseCameraManager* GenericMulticastDiscoveryManager::createCameraManager(const nxcip::CameraInfo& info)
{
    const nxcip::CameraInfo& infoCopy(info);
    return new GenericMulticastCameraManager(infoCopy);
}

int GenericMulticastDiscoveryManager::getReservedModelList(char** /*modelList*/, int* count)
{
    *count = 0;
    return nxcip::NX_NO_ERROR;
}
