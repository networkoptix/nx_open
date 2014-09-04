#include "camera_manager.h"
#include "discovery_manager.h"

#include <string>

#include <QtCore/QCryptographicHash>

//#include "../src/utils/common/log.h"

extern "C"
{
#ifdef _WIN32
    __declspec(dllexport)
#endif
    nxpl::PluginInterface* createNXPluginInstance()
    {
        return new pacidal::DiscoveryManager();
    }
}

namespace pacidal
{
    DEFAULT_REF_COUNTER(DiscoveryManager)

    DiscoveryManager* DiscoveryManager::Instance;

    DiscoveryManager::DiscoveryManager()
    :
        m_refManager( this )
    {
        Instance = this;
    }

    DiscoveryManager::~DiscoveryManager()
    {
        Instance = nullptr;
    }

    void* DiscoveryManager::queryInterface( const nxpl::NX_GUID& interfaceID )
    {
        if( interfaceID == nxcip::IID_CameraDiscoveryManager )
        {
            addRef();
            return this;
        }
        if( interfaceID == nxpl::IID_PluginInterface )
        {
            addRef();
            return static_cast<nxpl::PluginInterface*>(this);
        }

        return nullptr;
    }

    static const char* VENDOR_NAME = "Pacidal";
    static const char* MODEL_NAME = "DVB-T it930x (Endeavour)";

    void DiscoveryManager::getVendorName( char* buf ) const
    {
        strcpy( buf, VENDOR_NAME );
    }

    // TODO
    int DiscoveryManager::findCameras( nxcip::CameraInfo* cameras, const char* localInterfaceIPAddr )
    {
        std::string id;
        if (localInterfaceIPAddr)
        {
            const QByteArray uidStr = QCryptographicHash::hash(
                QByteArray::fromRawData(localInterfaceIPAddr, strlen(localInterfaceIPAddr)), QCryptographicHash::Md5 ).toHex();
            id = std::string(uidStr.constData(), uidStr.size());
        }

#if 0
        unsigned cameraNum = 1;

        memset( &cameras[0], 0, sizeof(cameras[0]) );
        strncpy( cameras[0].modelName, MODEL_NAME, std::min(strlen(MODEL_NAME), sizeof(nxcip::CameraInfo::modelName)-1) );
        strncpy( cameras[0].uid, id.c_str(), std::min(id.size(), sizeof(nxcip::CameraInfo::uid)-1) );
        strncpy( cameras[0].url, id.c_str(), std::min(id.size(), sizeof(nxcip::CameraInfo::url)-1) );
        cameras[0].auxiliaryData[0] = 1;
        cameras[0].auxiliaryData[1] = 0;
#else
        unsigned cameraNum = 4; // TODO: get from filesystem - /dev/usb-it930x*

        for (uint8_t i=0; i<cameraNum; ++i)
        {
            std::string camId = id;
            camId[0] = '0';
            camId[0] += i;

            memset( &cameras[i], 0, sizeof(cameras[i]) );
            strncpy( cameras[i].modelName, MODEL_NAME, std::min(strlen(MODEL_NAME), sizeof(nxcip::CameraInfo::modelName)-1) );
            strncpy( cameras[i].uid, camId.c_str(), std::min(camId.size(), sizeof(nxcip::CameraInfo::uid)-1) );
            strncpy( cameras[i].url, camId.c_str(), std::min(camId.size(), sizeof(nxcip::CameraInfo::uid)-1) );
            //cameras[i].auxiliaryData[0] = i;
            //cameras[i].auxiliaryData[0] = 0;
        }
#endif
        return cameraNum;
    }

    int DiscoveryManager::checkHostAddress( nxcip::CameraInfo* /*cameras*/, const char* /*address*/, const char* /*login*/, const char* /*password*/ )
    {
        return nxcip::NX_NOT_IMPLEMENTED;
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

    // TODO: lock?
    nxcip::BaseCameraManager* DiscoveryManager::createCameraManager( const nxcip::CameraInfo& info )
    {
        uint8_t id = info.uid[0] - '0';

        auto it = cameras_.find(id);
        if (it == cameras_.end())
        {
            cameras_[id] = std::make_shared<CameraManager>( info );
            return cameras_[id].get();
        }

        (it->second)->addRef();
        return it->second.get();
    }

    int DiscoveryManager::getReservedModelList( char** /*modelList*/, int* count )
    {
        *count = 0;
        return nxcip::NX_NO_ERROR;
    }
}
