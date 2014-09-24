#include "camera_manager.h"
#include "discovery_manager.h"

#include <dirent.h>
#include <string>

//#include <utils/common/log.h>

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
#if 0
    void debugPrintInfo(const nxcip::CameraInfo& info)
    {
        std::string s = std::string(info.url);
        s += " - ";
        s += std::string(info.modelName);
        s += " - ";
        s += std::string(info.firmware);
        NX_LOG( s.c_str(), cl_logDEBUG1 );
    }
#endif
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
        for (auto it = m_cameras.begin(); it != m_cameras.end(); ++it)
            it->second->releaseRef();

        Instance = nullptr;
    }

    void* DiscoveryManager::queryInterface( const nxpl::NX_GUID& interfaceID )
    {
        if (interfaceID == nxcip::IID_CameraDiscoveryManager)
        {
            addRef();
            return this;
        }
        if (interfaceID == nxpl::IID_PluginInterface)
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

    int DiscoveryManager::findCameras( nxcip::CameraInfo* cameras, const char* /*localInterfaceIPAddr*/ )
    {
        unsigned cameraNum = 0;

        DIR* dir = ::opendir( "/dev" );
        if (dir != NULL)
        {
            struct dirent * ent;
            while ((ent = ::readdir( dir )) != NULL)
            {
                nxcip::CameraInfo& info = cameras[cameraNum];

                std::string file( ent->d_name );
                if (file.find( CameraManager::DEVICE_PATTERN ) != std::string::npos)
                {
                    memset( &info, 0, sizeof(nxcip::CameraInfo) );
                    strncpy( info.url, file.c_str(), std::min(file.size(), sizeof(nxcip::CameraInfo::url)-1) );
                    strncpy( info.uid, file.c_str(), std::min(file.size(), sizeof(nxcip::CameraInfo::uid)-1) );

                    unsigned id = CameraManager::cameraId( info );
                    CameraManager * cam = nullptr;

                    {
                        std::lock_guard<std::mutex> lock( m_contentMutex ); // LOCK

                        auto it = m_cameras.find(id);
                        if (it == m_cameras.end())
                        {
                            // can't avoid lock for ctor
                            auto sp = std::make_shared<CameraManager>( info, false ); // create CameraManager
                            m_cameras[id] = sp;
                            cam = sp.get();
                            cam->addRef();
                        }
                        else
                            cam = it->second.get();
                    }

                    if (cam)
                    {
                        cam->getCameraInfo( &info );
                        if (!cam->hasEncoders())
                            cam->reload();
                    }

                    if (cam && cam->hasEncoders())
                        ++cameraNum;
                }
            }
            ::closedir( dir );
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
        CameraManager* cam = nullptr;

        {
            std::lock_guard<std::mutex> lock( m_contentMutex ); // LOCK

            auto it = m_cameras.find(id);
            if (it != m_cameras.end())
                cam = it->second.get();

            // before findCameras() on restart, use saved info from DB
            if (!cam)
            {
                // can't avoid lock for ctor
                auto sp = std::make_shared<CameraManager>( info /*, false*/ );  // create CameraManager
                m_cameras[id] = sp;
                cam = sp.get();
                cam->addRef();
            }
        }

        if (cam)
            cam->addRef();
        return cam;
    }

    int DiscoveryManager::getReservedModelList( char** /*modelList*/, int* count )
    {
        *count = 0;
        return nxcip::NX_NO_ERROR;
    }
}
