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
    //!
    class DiscoveryManager
    :
        public nxcip::CameraDiscoveryManager
    {
        DEF_REF_COUNTER

    public:
        DiscoveryManager();
        virtual ~DiscoveryManager();

        // nxcip::CameraDiscoveryManager

        virtual void getVendorName( char* buf ) const override;
        virtual int findCameras( nxcip::CameraInfo* cameras, const char* localInterfaceIPAddr ) override;
        virtual int checkHostAddress( nxcip::CameraInfo* cameras, const char* address, const char* login, const char* password ) override;
        virtual int fromMDNSData(
            const char* discoveredAddress,
            const unsigned char* mdnsResponsePacket,
            int mdnsResponsePacketSize,
            nxcip::CameraInfo* cameraInfo ) override;
        virtual int fromUpnpData( const char* upnpXMLData, int upnpXMLDataSize, nxcip::CameraInfo* cameraInfo ) override;
        virtual nxcip::BaseCameraManager* createCameraManager( const nxcip::CameraInfo& info ) override;
        virtual int getReservedModelList( char** modelList, int* count ) override;

        //

        static nxpt::CommonRefManager* refManager() { return &(Instance->m_refManager); }

    private:
        typedef std::shared_ptr<CameraManager> CameraPtr;

        mutable std::mutex m_contentMutex;
        std::map<unsigned, CameraPtr> m_cameras;

        static DiscoveryManager* Instance;
    };
}

#endif  //PACIDAL_DISCOVERY_MANAGER_H
