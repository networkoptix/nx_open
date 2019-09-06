// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <string>
#include <fstream>

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

namespace
{
    void getSerial(std::string& serial)
    {
        std::ifstream ifs("/proc/cpuinfo");

        std::string tmp;
        while (ifs.good())
        {
            ifs >> std::skipws >> tmp;
            if (tmp.find("Serial") != std::string::npos)
            {
                ifs >> std::skipws >> tmp;  // read ":"
                ifs >> std::skipws >> serial;
                break;
            }
        }

        debug_print("Serial: %s\n", serial.c_str());
    }
}

namespace rpi_cam
{
    DiscoveryManager * DiscoveryManager::Instance;

    DiscoveryManager::DiscoveryManager()
    :   m_camera(nullptr)
    {
        debug_print("%s\n", __FUNCTION__);

        RPiCamera::init();
        Instance = this;
    }

    DiscoveryManager::~DiscoveryManager()
    {
        debug_print("%s\n", __FUNCTION__);

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

    int DiscoveryManager::findCameras(nxcip::CameraInfo * cameras, const char * serverURL)
    {
        {
            std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

            if (! m_camera)
            {
                std::string url;
                if (serverURL)
                    url = serverURL;
                std::string serial;
                getSerial(serial);

                m_camera = new CameraManager(serial, url);
            }
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
        return nxcip::NX_NOT_IMPLEMENTED;
    }

    int DiscoveryManager::fromUpnpData( const char* /*upnpXMLData*/, int /*upnpXMLDataSize*/, nxcip::CameraInfo* /*cameraInfo*/ )
    {
        return nxcip::NX_NOT_IMPLEMENTED;
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
