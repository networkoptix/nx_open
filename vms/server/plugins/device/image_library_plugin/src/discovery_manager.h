// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <camera/camera_plugin.h>
#include <plugins/plugin_tools.h>

//!Represents defined (in settings) image directories as cameras with dts archive storage
class DiscoveryManager
:
    public nxcip::CameraDiscoveryManager3
{
public:
    DiscoveryManager();

    //!Implementation of nxpl::PluginInterface::queryInterface
    virtual void* queryInterface( const nxpl::NX_GUID& interfaceID ) override;
    //!Implementation of nxpl::PluginInterface::addRef
    virtual int addRef() const override;
    //!Implementation of nxpl::PluginInterface::releaseRef
    virtual int releaseRef() const override;

    //!Implementation of nxcip::CameraDiscoveryManager::getVendorName
    virtual void getVendorName( char* buf ) const override;
    //!Implementation of nxcip::CameraDiscoveryManager::findCameras
    virtual int findCameras( nxcip::CameraInfo* cameras, const char* localInterfaceIPAddr ) override;
    //!Implementation of nxcip::CameraDiscoveryManager::checkHostAddress
    virtual int checkHostAddress( nxcip::CameraInfo* cameras, const char* address, const char* login, const char* password ) override;
    //!Implementation of nxcip::CameraDiscoveryManager2::checkHostAddress
    virtual int checkHostAddress2(nxcip::CameraInfo2* cameras, const char* address, const char* login, const char* password) override;
    //!Implementation of nxcip::CameraDiscoveryManager2::findCameras2
    virtual int findCameras2(nxcip::CameraInfo2* cameras, const char* serverURL) override;

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

    //!Implementation of nxcip::CameraDiscoveryManager3::getCapabilities
    virtual int getCapabilities() const override;

private:
    nxpt::CommonRefManager m_refManager;
};
