#include "camera_manager.h"
#include "discovery_manager.h"

#include <dirent.h>
#include <string>

#include "camera_manager.h"

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

namespace
{
    static const char* VENDOR_NAME = "Pacidal";
    static const char* MODEL_NAME = "DVB-T it930x (Endeavour)";
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

    void DiscoveryManager::getVendorName( char* buf ) const
    {
        strcpy( buf, VENDOR_NAME );
    }

    // BUG v.2.2 Restore only modelName and url fields in CameraInfo after restart
    int DiscoveryManager::findCameras( nxcip::CameraInfo* cameras, const char* /*localInterfaceIPAddr*/ )
    {
        unsigned cameraNum = 0;

        DIR* dir = ::opendir( "/dev" );
        if( dir != NULL )
        {
            struct dirent * ent;
            while( (ent = ::readdir( dir )) != NULL )
            {
                std::string file( ent->d_name );
                if (file.find( CameraManager::DEVICE_PATTERN ) != std::string::npos)
                {
                    memset( &cameras[cameraNum], 0, sizeof(cameras[cameraNum]) );
                    strncpy( cameras[cameraNum].modelName, MODEL_NAME, std::min(strlen(MODEL_NAME), sizeof(nxcip::CameraInfo::modelName)-1) );
                    strncpy( cameras[cameraNum].uid, file.c_str(), std::min(file.size(), sizeof(nxcip::CameraInfo::uid)-1) );
                    strncpy( cameras[cameraNum].url, file.c_str(), std::min(file.size(), sizeof(nxcip::CameraInfo::uid)-1) );
                    ++cameraNum;
                }
            }
        }

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

    nxcip::BaseCameraManager* DiscoveryManager::createCameraManager( const nxcip::CameraInfo& info )
    {
        unsigned id = CameraManager::cameraId( info );

        std::lock_guard<std::mutex> lock(m_mutex); // LOCK

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
