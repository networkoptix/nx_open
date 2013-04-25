/**********************************************************
* 2 apr 2013
* akolesnikov
***********************************************************/

#ifndef AXIS_DISCOVERY_MANAGER_H
#define AXIS_DISCOVERY_MANAGER_H

#include <plugins/camera_plugin.h>

#include "common_ref_manager.h"


//
/*!
    Implements only MDNS search method (\a nxcip::CameraDiscoveryManager::fromMDNSData method)

    \note Delegates reference counting to \a AxisCameraPlugin instance
*/
class AxisCameraDiscoveryManager
:
    public CommonRefManager,
    public nxcip::CameraDiscoveryManager
{
public:
    AxisCameraDiscoveryManager();
    virtual ~AxisCameraDiscoveryManager();

    //!Implementaion of nxpl::NXPluginInterface::queryInterface
    virtual void* queryInterface( const nxpl::NX_GUID& interfaceID ) override;
    //!Implementaion of nxpl::NXPluginInterface::addRef
    virtual unsigned int addRef() override;
    //!Implementaion of nxpl::NXPluginInterface::releaseRef
    virtual unsigned int releaseRef() override;

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
    //!Implementation of nxcip::CameraDiscoveryManager::getReservedModelListFirst
    /*!
        Does nothing
    */
    virtual void getReservedModelListFirst( char** modelList, int* count ) override;
    //!Implementation of nxcip::CameraDiscoveryManager::getReservedModelListFirst
    /*!
        Does nothing
    */
    virtual void getReservedModelListNext( char** modelList, int* count ) override;
};

#endif  //AXIS_DISCOVERY_MANAGER_H
