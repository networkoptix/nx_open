#include "rpi_camera.h"

#include "camera_manager.h"
#include "discovery_manager.h"

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
    :   m_refManager(this),
        m_camera(nullptr)
    {
        debug_print("DiscoveryManager()\n");

        RPiCamera::init();
        Instance = this;
    }

    DiscoveryManager::~DiscoveryManager()
    {
        debug_print("~DiscoveryManager()\n");

        if (m_camera)
            m_camera->releaseRef();

        Instance = nullptr;
        RPiCamera::deinit();
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

    int DiscoveryManager::findCameras(nxcip::CameraInfo * cameras, const char * /*localInterfaceIPAddr*/)
    {
        if (! m_camera)
        {
            std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

            if (! m_camera)
                m_camera = new CameraManager();
        }

        unsigned cameraNum = 0;
        if (m_camera)
        {
            nxcip::CameraInfo& info = cameras[0];
            m_camera->getCameraInfo(&info);
            ++cameraNum;
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

    nxcip::BaseCameraManager* DiscoveryManager::createCameraManager(const nxcip::CameraInfo& )
    {
        if (m_camera)
        {
            m_camera->addRef();
            return m_camera;
        }

        return nullptr;
    }

    int DiscoveryManager::getReservedModelList( char** /*modelList*/, int* count )
    {
        *count = 0;
        return nxcip::NX_NO_ERROR;
    }
}
