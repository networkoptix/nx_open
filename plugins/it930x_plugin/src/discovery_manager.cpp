#include <vector>
#include <cassert>
#include <future>

#include <dirent.h>

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
    //
    const int RESCAN_TIMEOUT = 300 * 1000; // ms

    DiscoveryManager * DiscoveryManager::Instance;

    DiscoveryManager::DiscoveryManager()
    :   m_needScan(true),
        m_blockAutoScan(false),
        m_firstFind(true)
    {
        Instance = this;
        m_rescanTimer.restart();
    }

    DiscoveryManager::~DiscoveryManager()
    {
        // release CameraManagers before DeviceMapper
        for (auto it = m_cameras.begin(); it != m_cameras.end(); ++it)
            (*it)->releaseRef();

        Instance = nullptr;
    }

    void * DiscoveryManager::queryInterface( const nxpl::NX_GUID& interfaceID )
    {
//        if (interfaceID == nxcip::IID_CameraDiscoveryManager2)
//        {
//            addRef();
//            return this;
//        }
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
        static const char * VENDOR_NAME = "it930x";
        strcpy( buf, VENDOR_NAME );
    }

    int DiscoveryManager::fromMDNSData(
        const char* /*discoveredAddress*/,
        const unsigned char* /*mdnsResponsePacket*/,
        int /*mdnsResponsePacketSize*/,
        nxcip::CameraInfo* /*cameraInfo*/ )
    {
        return nxcip::NX_NOT_IMPLEMENTED;
    }

    int DiscoveryManager::fromUpnpData( const char* /*upnpXMLData*/, int /*upnpXMLDataSize*/, nxcip::CameraInfo* /*cameraInfo*/ )
    {
        return nxcip::NX_NOT_IMPLEMENTED;
    }

    int DiscoveryManager::getReservedModelList( char** /*modelList*/, int* /*count*/ )
    {
        return nxcip::NX_NOT_IMPLEMENTED;
    }

    static const char * internal = "HACK";

    int DiscoveryManager::checkHostAddress( nxcip::CameraInfo* cameras, const char* /*address*/, const char* /*login*/, const char* /*password*/ )
    {
        return findCameras(cameras, internal);
    }

    int DiscoveryManager::findCameras(nxcip::CameraInfo * cameras, const char * opt)
    {
        // refactor
        if (m_needScan)
        {
            m_needScan = false;
            scan_2();
        }

        int cameraCount = 0;

        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto it = m_rxDevices.begin(); it != m_rxDevices.end(); ++it)
        {
            debug_printf("[DiscoveryManager::findCameras] trying RX %d\n", (*it)->rxID());
            if ((*it)->deviceReady())
            {
                debug_printf("[DiscoveryManager::findCameras] RX %d is good!\n", (*it)->rxID());
                nxcip::CameraInfo info = (*it)->getCameraInfo();
                debug_printf(
                    "[DiscoveryManager::findCameras] Rx: %d; cam uid: %s cam url: %s\n",
                    (*it)->rxID(),
                    info.uid,
                    info.url
                );

                cameras[cameraCount++] = info;
            }
            else
                debug_printf("[DiscoveryManager::findCameras] cam %d is not good!\n", (*it)->rxID());
        }
        return cameraCount;
        // <refactor
    }

    void DiscoveryManager::findAndSwapCamerasRx(CameraPtr cam1, CameraPtr cam2)
    {
        std::lock(cam1->get_mutex(), cam2->get_mutex());
        std::lock_guard<std::mutex> lk1(cam1->get_mutex(), std::adopt_lock);
        std::lock_guard<std::mutex> lk2(cam2->get_mutex(), std::adopt_lock);

        cam1->rxDeviceRef().swap(cam2->rxDeviceRef());
    }

    nxcip::BaseCameraManager * DiscoveryManager::createCameraManager( const nxcip::CameraInfo& info )
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        debug_printf(
            "[DiscoveryManager::createCameraManager] We have %lu cameras\n",
            m_cameras.size()
        );

        unsigned short txID = RxDevice::str2id(info.uid);
        CameraManager * cam = nullptr;
        debug_printf("[DiscoveryManager::createCameraManager] trying to get cam %d\n", txID);

        auto cameraIt = std::find_if(
            m_cameras.begin(),
            m_cameras.end(),
            [txID](CameraPtr c)
            {
                return c->cameraId() == txID;
            }
        );

        auto rxIt = std::find_if(
            m_rxDevices.begin(),
            m_rxDevices.end(),
            [txID](RxDevicePtr p)
            {
                return p->getTxDevice() && p->getTxDevice()->txID() == txID;
            }
        );

        if (rxIt == m_rxDevices.end())
        {
            debug_printf("[DiscoveryManager::createCameraManager] no camera found\n");
            if (cameraIt != m_cameras.end() && (*cameraIt)->rxDeviceRef())
            {
                debug_printf("[DiscoveryManager::createCameraManager] releasing cameraManager rx\n");
                std::lock_guard<std::mutex> lock((*cameraIt)->get_mutex());
                (*cameraIt)->rxDeviceRef().reset();
            }
            return nullptr;
        }

        if (cameraIt != m_cameras.end())
        {
            if ((*cameraIt)->rxDeviceRef())
            {
                if ((*cameraIt)->rxDeviceRef() == *rxIt)
                {
                    debug_printf("[DiscoveryManager::createCameraManager] camera rxDevice already correct. Camera found!\n");
                }
                else
                {
                    debug_printf("[DiscoveryManager::createCameraManager] camera rxDevice is not correct, replacing. Camera found!\n");
                    std::lock_guard<std::mutex> lock((*cameraIt)->get_mutex());
                    (*cameraIt)->rxDeviceRef().reset();
                    (*cameraIt)->rxDeviceRef() = *rxIt;
                    (*cameraIt)->rxDeviceRef()->setCamera((*cameraIt).get());
                }
                cam = cameraIt->get();
                cam->addRef();
                return cam;
            }
            else
            {
                debug_printf("[DiscoveryManager::createCameraManager] No rxDevice for camera. Setting one and returning. Camera found!\n");
                std::lock_guard<std::mutex> lock((*cameraIt)->get_mutex());
                (*cameraIt)->rxDeviceRef() = *rxIt;
                (*cameraIt)->rxDeviceRef()->setCamera((*cameraIt).get());

                cam = cameraIt->get();
                cam->addRef();
                return cam;
            }
        }
        else
        {
            debug_printf(
                "[DiscoveryManager::createCameraManager] creating new camera with rx device %d. Camera found!\n",
                (*rxIt)->rxID()
            );
            CameraPtr newCam(new CameraManager(*rxIt));
            m_cameras.push_back(newCam);
            cam = newCam.get();
            cam->addRef();
            return cam;
        }

        return cam;
    }

//    int DiscoveryManager::notify(uint32_t event, uint32_t value, void * )
//    {
//        assert(0);
//        switch (event)
//        {
//            case EVENT_SEARCH:
//            {
//                if (value == EVENT_SEARCH_NEW_SEARCH)
//                {
//                    //m_needScan = true;
//                    m_blockAutoScan = true;
//                }

//                return nxcip::NX_NO_ERROR;
//            }

//            default:
//                break;
//        }

//        return nxcip::NX_NO_ERROR;
//    }

    void getRxDevNames(std::vector<std::string>& devs)
    {
        devs.clear();
        DIR * dir = ::opendir( "/dev" );

        if (dir != NULL)
        {
            struct dirent * ent;
            while ((ent = ::readdir( dir )) != NULL)
            {
                std::string file( ent->d_name );
                if (file.find( RxDevice::DEVICE_PATTERN ) != std::string::npos)
                    devs.push_back(file);
            }
            ::closedir( dir );
        }
    }

    void DiscoveryManager::scan_2()
    {
        debug_printf("[scan_2] started\n");

        // Getting available Rx devices
        // This is done once since they should not
        // be altered while server is up.
        std::vector<std::string> names;
        getRxDevNames(names);

        for (size_t i = 0; i < names.size(); ++i)
        {
            unsigned rxID = RxDevice::dev2id(names[i]);
            RxDevicePtr newRxDevice(new RxDevice(rxID));
            newRxDevice->startWatchDog();
            m_rxDevices.push_back(newRxDevice);
        }
    }
}
