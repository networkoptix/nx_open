#pragma once

#include <vector>

#include <camera/camera_plugin.h>
#include <plugins/plugin_tools.h>
#include <plugins/plugin_container_api.h>

namespace nx {
namespace webcam_plugin {

class DiscoveryManager
:
    public nxcip::CameraDiscoveryManager
{
public:
    DiscoveryManager(nxpt::CommonRefManager* const refManager,
                     nxpl::TimeProvider *const timeProvider);

    virtual void* queryInterface( const nxpl::NX_GUID& interfaceID ) override;
    virtual unsigned int addRef() override;
    virtual unsigned int releaseRef() override;

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
    /*!
        Does nothing
    */
    virtual int getReservedModelList( char** modelList, int* count ) override;

private:
    nxpt::CommonRefManager m_refManager;
    nxpl::TimeProvider *const m_timeProvider;
};

} // namespace nx 
} // namespace webcam_plugin 
