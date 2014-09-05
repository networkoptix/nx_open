#ifndef PACIDAL_DISCOVERY_MANAGER_H
#define PACIDAL_DISCOVERY_MANAGER_H

#include <memory>
#include <map>
#include <mutex>

#include <plugins/camera_plugin.h>

#include "ref_counter.h"
#include "camera_manager.h"

namespace pacidal
{
    //!Represents defined (in settings) image directories as cameras with dts archive storage
    class DiscoveryManager
    :
        public nxcip::CameraDiscoveryManager
    {
        DEF_REF_COUNTER

    public:
        DiscoveryManager();
        virtual ~DiscoveryManager();

        //!Implementation of nxcip::CameraDiscoveryManager::getVendorName
        virtual void getVendorName( char* buf ) const override;
        //!Implementation of nxcip::CameraDiscoveryManager::findCameras
        virtual int findCameras( nxcip::CameraInfo* cameras, const char* localInterfaceIPAddr ) override;
        //!Implementation of nxcip::CameraDiscoveryManager::checkHostAddress
        virtual int checkHostAddress( nxcip::CameraInfo* cameras, const char* address, const char* login, const char* password ) override;
        //!Implementation of nxcip::CameraDiscoveryManager::fromMDNSData
        virtual int fromMDNSData(
            const char* discoveredAddress,
            const unsigned char* mdnsResponsePacket,
            int mdnsResponsePacketSize,
            nxcip::CameraInfo* cameraInfo ) override;
        //!Implementation of nxcip::CameraDiscoveryManager::fromUpnpData
        virtual int fromUpnpData( const char* upnpXMLData, int upnpXMLDataSize, nxcip::CameraInfo* cameraInfo ) override;
        //!Implementation of nxcip::CameraDiscoveryManager::createCameraManager
        virtual nxcip::BaseCameraManager* createCameraManager( const nxcip::CameraInfo& info ) override;
        //!Implementation of nxcip::CameraDiscoveryManager::getReservedModelList
        /*!
            Does nothing
        */
        virtual int getReservedModelList( char** modelList, int* count ) override;

        static nxpt::CommonRefManager* refManager() { return &(Instance->m_refManager); }

    private:
        typedef std::shared_ptr<CameraManager> CameraPtr;

        static DiscoveryManager* Instance;
        std::map<unsigned, CameraPtr> m_cameras;
        mutable std::mutex m_mutex;
        bool m_firstTime;
    };
}

#endif  //PACIDAL_DISCOVERY_MANAGER_H
