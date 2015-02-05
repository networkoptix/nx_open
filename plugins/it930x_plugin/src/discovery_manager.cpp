#include <vector>

#include "discovery_manager.h"
#include "camera_manager.h"

extern "C"
{
#ifdef _WIN32
    __declspec(dllexport)
#endif
    nxpl::PluginInterface * createNXPluginInstance()
    {
        return new ite::DiscoveryManager();
    }
}

namespace ite
{
    static const char * VENDOR_NAME = "ITE";

    //

    DEFAULT_REF_COUNTER(DiscoveryManager)

    DiscoveryManager * DiscoveryManager::Instance;

    DiscoveryManager::DiscoveryManager()
    :   m_refManager( this )
    {
        Instance = this;
    }

    DiscoveryManager::~DiscoveryManager()
    {
        // release CameraManagers before DeviceMapper
        for (auto it = m_cameras.begin(); it != m_cameras.end(); ++it)
            it->second->releaseRef();

        Instance = nullptr;
    }

    void * DiscoveryManager::queryInterface( const nxpl::NX_GUID& interfaceID )
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

    int DiscoveryManager::getReservedModelList( char** /*modelList*/, int* count )
    {
        *count = 0;
        return nxcip::NX_NO_ERROR;
    }

    void DiscoveryManager::getVendorName( char* buf ) const
    {
        strcpy( buf, VENDOR_NAME );
    }

    int DiscoveryManager::findCameras(nxcip::CameraInfo * cameras, const char * /*localInterfaceIPAddr*/)
    {
        unsigned cameraNum = 0;

        std::vector<TxDevicePtr> txDevs;
        m_devMapper.txDevs(txDevs);

        for (auto it = txDevs.begin(); it != txDevs.end(); ++it)
        {
            unsigned txID = (*it)->txID();

            nxcip::CameraInfo& info = cameras[cameraNum];
            m_devMapper.makeInfo(info, txID);

            CameraManager * cam = nullptr;

            {
                std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

                auto itCam = m_cameras.find(txID);
                if (itCam == m_cameras.end())
                {
                    auto sp = std::make_shared<CameraManager>(info, &m_devMapper); // create CameraManager
                    m_cameras[txID] = sp;
                    cam = sp.get();
                    cam->addRef();
                }
                else
                    cam = itCam->second.get();
            }

            if (cam)
            {
                // delayed receivers release
                if (cam->stopIfNeedIt())
                    continue; // HACK: hide camera once

                cam->updateCameraInfo(info);
                ++cameraNum;
            }
        }

        return cameraNum;
    }

    nxcip::BaseCameraManager * DiscoveryManager::createCameraManager( const nxcip::CameraInfo& info )
    {
        unsigned short txID = RxDevice::str2id(info.uid);
        CameraManager * cam = nullptr;

        {
            std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

            auto it = m_cameras.find(txID);
            if (it != m_cameras.end())
                cam = it->second.get();
        }

        // before findCameras() on restart, use saved info from DB
        if (!cam)
        {
            {
                std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

                auto sp = std::make_shared<CameraManager>(info, &m_devMapper);  // create CameraManager
                m_cameras[txID] = sp;
                cam = sp.get();
                cam->addRef();

                m_devMapper.restoreCamera(info);
            }
        }

        if (cam)
            cam->addRef();
        return cam;
    }
}
