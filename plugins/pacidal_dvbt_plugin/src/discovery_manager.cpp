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
}

namespace pacidal
{
    DEFAULT_REF_COUNTER(DiscoveryManager)

    DiscoveryManager* DiscoveryManager::Instance;

    DiscoveryManager::DiscoveryManager()
    :
        m_refManager( this ),
        m_firstTime( true )
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
                if( file.find( CameraManager::DEVICE_PATTERN ) != std::string::npos )
                {
                    memset( &cameras[cameraNum], 0, sizeof(cameras[cameraNum]) );
                    strncpy( cameras[cameraNum].url, file.c_str(), std::min(file.size(), sizeof(nxcip::CameraInfo::url)-1) );
                    strncpy( cameras[cameraNum].uid, file.c_str(), std::min(file.size(), sizeof(nxcip::CameraInfo::uid)-1) );

                    unsigned id = CameraManager::cameraId( cameras[cameraNum] );
                    bool alreadyFound = false;

                    {
                        std::lock_guard<std::mutex> lock(m_mutex); // LOCK

                        auto it = m_cameras.find(id);
                        if( it != m_cameras.end() )
                        {
                            //it->second->fillInfo( cameras[cameraNum] );
                            ++cameraNum;
                            alreadyFound = true;
                        }
                    }

                    if( m_firstTime && !alreadyFound )
                    {
                        CameraManager cam( cameras[cameraNum], true );
                        if( cam.devInitialised() )
                        {
                            cam.fillInfo( cameras[cameraNum] );
                            ++cameraNum;
                        }
                    }
                }
            }
            ::closedir( dir );
        }

        m_firstTime = false;
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

        auto it = m_cameras.find(id);
        if (it == m_cameras.end())
        {
            m_cameras[id] = std::make_shared<CameraManager>( info );
            return m_cameras[id].get();
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
