#include <string>

#include "camera_manager.h"
#include "discovery_manager.h"

#include "bcm_host.h"

#include "camera_manager.h"

extern "C"
{
    nxpl::PluginInterface* createNXPluginInstance()
    {
        return new rpi_cam::DiscoveryManager();
    }
}

namespace rpi_cam
{
    DEFAULT_REF_COUNTER(DiscoveryManager)

    DiscoveryManager * DiscoveryManager::Instance;

    DiscoveryManager::DiscoveryManager()
    :   m_refManager( this )
    {
        Instance = this;

        bcm_host_init();
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

    void DiscoveryManager::getVendorName( char * buf ) const
    {
        static const char * VENDOR_NAME = "Raspberry Pi";
        strcpy(buf, VENDOR_NAME);
    }

    int DiscoveryManager::findCameras( nxcip::CameraInfo* cameras, const char* /*localInterfaceIPAddr*/ )
    {
        static const unsigned MAX_CAMERAS_NUM = 1; // TODO: up to 2

        unsigned cameraNum = 0;
        for (unsigned id = 0; id < MAX_CAMERAS_NUM; ++id)
        {
            nxcip::CameraInfo& info = cameras[cameraNum];

            CameraManager * cam = nullptr;

            {
                std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

                auto it = m_cameras.find(id);
                if (it == m_cameras.end())
                {
                    auto sp = std::make_shared<CameraManager>(id); // create CameraManager
                    m_cameras[id] = sp;
                    cam = sp.get();
                    cam->addRef();
                }
                else
                    cam = it->second.get();
            }

            if (cam && cam->isOK())
            {
                cam->getCameraInfo(&info);
                ++cameraNum;
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
        unsigned id = 0; // TODO
        CameraManager * cam = nullptr;

        {
            std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

            auto it = m_cameras.find(id);
            if (it != m_cameras.end())
                cam = it->second.get();

            // before findCameras() on restart, use saved info from DB
            if (!cam)
            {
                auto sp = std::make_shared<CameraManager>(id);  // create CameraManager
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
