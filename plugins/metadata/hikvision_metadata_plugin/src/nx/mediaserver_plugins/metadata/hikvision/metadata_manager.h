#pragma once

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtCore/QString>
#include <QtNetwork/QAuthenticator>

#include <nx/utils/thread/mutex.h>

#include "metadata_monitor.h"
#include "metadata_plugin.h"

#include <plugins/plugin_tools.h>
#include <nx/sdk/metadata/camera_manager.h>
#include <nx/utils/url.h>

namespace nx {
namespace mediaserver {
namespace plugins {
namespace hikvision {

// TODO: Rename and change namespaces the same way as e.g. Stub plugin.
class MetadataManager:
    public QObject,
    public nxpt::CommonRefCounter<nx::sdk::metadata::CameraManager>
{
    Q_OBJECT

public:
    MetadataManager(MetadataPlugin* plugin);

    virtual ~MetadataManager();

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    virtual nx::sdk::Error startFetchingMetadata(
        const char* const* typeList, int typeListSize) override;

    virtual nx::sdk::Error setHandler(nx::sdk::metadata::MetadataHandler* handler) override;

    virtual nx::sdk::Error stopFetchingMetadata() override;

    virtual const char* capabilitiesManifest(nx::sdk::Error* error) override;

    virtual void freeManifest(const char* data) override;

    void setCameraInfo(const nx::sdk::CameraInfo& cameraInfo);
    void setDeviceManifest(const QByteArray& manifest);
    void setDriverManifest(const Hikvision::DriverManifest& manifest);
    void setDeclaredSettings(const nxpl::Setting* settings, int count) override;

private:
    Hikvision::DriverManifest m_driverManifest;
    QByteArray m_deviceManifest;

    nx::utils::Url m_url;
    QString m_model;
    QString m_firmware;
    QAuthenticator m_auth;
    QString m_uniqueId;
    QString m_sharedId;
    int m_channel = 0;

    MetadataPlugin* m_plugin = nullptr;
    std::unique_ptr<HikvisionMetadataMonitor> m_monitor;
    nx::sdk::metadata::MetadataHandler* m_handler = nullptr;
};

} // namespace hikvision
} // namespace plugins
} // namespace mediaserver
} // namespace nx
