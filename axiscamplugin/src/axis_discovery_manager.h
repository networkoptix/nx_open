/**********************************************************
* 2 apr 2013
* akolesnikov
***********************************************************/

#ifndef AXIS_DISCOVERY_MANAGER_H
#define AXIS_DISCOVERY_MANAGER_H

#include <QAtomicInt>

#include <plugins/camera_plugin.h>

#include "common_ref_manager.h"


//
/*!
    Implements only MDNS search method (\a nxcip::CameraDiscoveryManager::fromMDNSData method)
*/
class AxisCameraDiscoveryManager
:
    public CommonRefManager,
    public nxcip::CameraDiscoveryManager
{
public:
    AxisCameraDiscoveryManager();

    virtual void* queryInterface( const nxpl::NX_GUID& interfaceID ) override;
    //virtual unsigned int addRef() override;
    //virtual unsigned int releaseRef() override;

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

//private:
//    QAtomicInt m_refCount;
};

#endif  //AXIS_DISCOVERY_MANAGER_H
