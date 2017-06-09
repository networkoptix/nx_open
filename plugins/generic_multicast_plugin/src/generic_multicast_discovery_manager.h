#pragma once

#include <plugins/camera_plugin.h>
#include <plugins/plugin_tools.h>

class GenericMulticastDiscoveryManager
:
    public nxcip::CameraDiscoveryManager
{
public:
    GenericMulticastDiscoveryManager();

    /** Implementation of nxpl::PluginInterface::queryInterface */
    virtual void* queryInterface(const nxpl::NX_GUID& interfaceID) override;
    /** Implementation of nxpl::PluginInterface::addRef */
    virtual unsigned int addRef() override;
    /** Implementation of nxpl::PluginInterface::releaseRef */
    virtual unsigned int releaseRef() override;

    /** Implementation of nxcip::CameraDiscoveryManager::getVendorName */
    virtual void getVendorName(char* buf) const override;
    /** Implementation of nxcip::CameraDiscoveryManager::findCameras */
    virtual int findCameras(nxcip::CameraInfo* cameras, const char* localInterfaceIPAddr) override;
    /** Implementation of nxcip::CameraDiscoveryManager::checkHostAddress */
    virtual int checkHostAddress(
        nxcip::CameraInfo* cameras,
        const char* address,
        const char* login,
        const char* password) override;

    /** Implementation of nxcip::CameraDiscoveryManager::fromMDNSData */
    virtual int fromMDNSData(
        const char* discoveredAddress,
        const unsigned char* mdnsResponsePacket,
        int mdnsResponsePacketSize,
        nxcip::CameraInfo* cameraInfo ) override;
    /** Implementation of nxcip::CameraDiscoveryManager::fromUpnpData */
    virtual int fromUpnpData( const char* upnpXMLData, int upnpXMLDataSize, nxcip::CameraInfo* cameraInfo ) override;
    /** Implementation of nxcip::CameraDiscoveryManager::createCameraManager */
    virtual nxcip::BaseCameraManager* createCameraManager( const nxcip::CameraInfo& info ) override;
    /** Implementation of nxcip::CameraDiscoveryManager::getReservedModelList */
    virtual int getReservedModelList( char** modelList, int* count ) override;

private:
    nxpt::CommonRefManager m_refManager;
};
