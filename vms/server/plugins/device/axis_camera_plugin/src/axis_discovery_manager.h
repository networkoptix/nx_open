/**********************************************************
* 2 apr 2013
* akolesnikov
***********************************************************/

#ifndef AXIS_DISCOVERY_MANAGER_H
#define AXIS_DISCOVERY_MANAGER_H

#include <camera/camera_plugin.h>

#include <plugins/plugin_tools.h>


//!Discovers AXIS cameras with MDNS search method (implements \a nxcip::CameraDiscoveryManager)
/*!
    Implements only MDNS search method (\a nxcip::CameraDiscoveryManager::fromMDNSData method)

    \note Delegates reference counting to \a AxisCameraPlugin instance
*/
class AxisCameraDiscoveryManager
:
    public nxcip::CameraDiscoveryManager
{
public:
    AxisCameraDiscoveryManager();
    virtual ~AxisCameraDiscoveryManager();

    //!Implementaion of nxpl::PluginInterface::queryInterface
    virtual void* queryInterface( const nxpl::NX_GUID& interfaceID ) override;
    //!Implementaion of nxpl::PluginInterface::addRef
    virtual int addRef() const override;
    //!Implementaion of nxpl::PluginInterface::releaseRef
    virtual int releaseRef() const override;

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
    virtual int getReservedModelList( char** modelList, int* count ) override;

private:
    nxpt::CommonRefManager m_refManager;
};

#endif  //AXIS_DISCOVERY_MANAGER_H
