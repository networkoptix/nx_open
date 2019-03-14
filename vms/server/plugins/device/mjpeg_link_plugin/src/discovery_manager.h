#pragma once

#include <vector>

#include <QtCore/QString>

#include <nx/utils/url.h>

#include <camera/camera_plugin.h>
#include <plugins/plugin_tools.h>
#include <plugins/plugin_container_api.h>

struct UrlPathReplaceRecord
{
    QString fromPath;
    QString toPath;
};

namespace nx::vms_server_plugins::mjpeg_link {

//!Represents defined (in settings) image directories as cameras with dts archive storage
class DiscoveryManager: public nxcip::CameraDiscoveryManager2
{
public:
    DiscoveryManager(nxpt::CommonRefManager* const refManager,
                     nxpl::TimeProvider *const timeProvider);

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
    //!Implementation of nxcip::CameraDiscoveryManager2::findCameras
    virtual int findCameras2(nxcip::CameraInfo2* cameras, const char* localInterfaceIPAddr) override;

    //!Implementation of nxcip::CameraDiscoveryManager::checkHostAddress
    virtual int checkHostAddress(nxcip::CameraInfo* cameras, const char* address, const char* login, const char* password ) override;
    //!Implementation of nxcip::CameraDiscoveryManager2::checkHostAddress
    virtual int checkHostAddress2(nxcip::CameraInfo2* cameras, const char* address, const char* login, const char* password) override;

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
    QList<nx::utils::Url> translateUrlHook(const nx::utils::Url& url) const;
    QString getGroupName(const nx::utils::Url& url) const;
    static bool validateUrl(const nx::utils::Url& url);
private:
    nxpt::CommonRefManager m_refManager;
    nxpl::TimeProvider *const m_timeProvider;
    std::vector<UrlPathReplaceRecord> m_replaceData;
};

} // namespace nx::vms_server_plugins::mjpeg_link
