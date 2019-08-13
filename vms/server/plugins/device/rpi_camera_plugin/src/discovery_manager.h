// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef RPI_DISCOVERY_MANAGER_H
#define RPI_DISCOVERY_MANAGER_H

#include <mutex>

#include <camera/camera_plugin.h>

#include "ref_counter.h"

namespace rpi_cam
{
    class CameraManager;

    //!
    class DiscoveryManager : public DefaultRefCounter<nxcip::CameraDiscoveryManager>
    {
    public:
        DiscoveryManager();
        virtual ~DiscoveryManager();

        // nxpl::PluginInterface

        virtual void * queryInterface( const nxpl::NX_GUID& interfaceID ) override;

        // nxcip::CameraDiscoveryManager

        virtual void getVendorName( char* buf ) const override;
        virtual int findCameras( nxcip::CameraInfo* cameras, const char* serverURL ) override;
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

        static nxpt::CommonRefManager * refManager() { return &(Instance->m_refManager); }

    private:
        mutable std::mutex m_mutex;
        CameraManager * m_camera;

        static DiscoveryManager * Instance;
    };
}

#endif
