/**********************************************************
* 03 sep 2013
* akolesnikov
***********************************************************/

#ifndef ILP_DISCOVERY_MANAGER_H
#define ILP_DISCOVERY_MANAGER_H

#include <plugins/camera_plugin.h>

#include <QtCore/QByteArray>

#include <plugins/plugin_tools.h>


//!Represents defined (in settings) image directories as cameras with dts archive storage
class DiscoveryManager
:
    public nxcip::CameraDiscoveryManager
{
public:
    DiscoveryManager();

    //!Implementation of nxpl::PluginInterface::queryInterface
    virtual void* queryInterface( const nxpl::NX_GUID& interfaceID ) override;
    //!Implementaion of nxpl::PluginInterface::addRef
    virtual unsigned int addRef() override;
    //!Implementaion of nxpl::PluginInterface::releaseRef
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
    //!Implementation of nxcip::CameraDiscoveryManager::getReservedModelList
    /*!
        Does nothing
    */
    virtual int getReservedModelList( char** modelList, int* count ) override;

private:
    nxpt::CommonRefManager m_refManager;
    QByteArray m_firmwareVersion;
    QByteArray m_modelName;
};

#endif  //ILP_DISCOVERY_MANAGER_H
