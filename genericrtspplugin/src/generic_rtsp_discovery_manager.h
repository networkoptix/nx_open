/**********************************************************
* 22 apr 2013
* akolesnikov
***********************************************************/

#ifndef GENERIC_RTSP_DISCOVERY_MANAGER_H
#define GENERIC_RTSP_DISCOVERY_MANAGER_H

#include <plugins/camera_plugin.h>

#include "common_ref_manager.h"


/*!
    Supports only url check. Also, instansiates camera manager
*/
class GenericRTSPDiscoveryManager
:
    public CommonRefManager,
    public nxcip::CameraDiscoveryManager
{
public:
    GenericRTSPDiscoveryManager();

    //!Implementation of nxpl::NXPluginInterface::queryInterface
    virtual void* queryInterface( const nxpl::NX_GUID& interfaceID ) override;

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

#endif  //GENERIC_RTSP_DISCOVERY_MANAGER_H
