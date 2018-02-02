#pragma once

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtCore/QString>
#include <QtNetwork/QAuthenticator>

#include <nx/utils/thread/mutex.h>

#include <hanwha_metadata_monitor.h>
#include <hanwha_metadata_plugin.h>

#include <plugins/plugin_tools.h>
#include <nx/sdk/metadata/camera_manager.h>

namespace nx {
namespace mediaserver {
namespace plugins {

class HanwhaMetadataManager:
    public QObject,
    public nxpt::CommonRefCounter<nx::sdk::metadata::CameraManager>
{
    Q_OBJECT

public:
    HanwhaMetadataManager(HanwhaMetadataPlugin* plugin);

    virtual ~HanwhaMetadataManager();

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    virtual nx::sdk::Error startFetchingMetadata(
        nx::sdk::metadata::MetadataHandler* handler,
        nxpl::NX_GUID* typeList,
        int typeListSize) override;

    virtual nx::sdk::Error stopFetchingMetadata() override;

    virtual const char* capabilitiesManifest(nx::sdk::Error* error) override;

    virtual void freeManifest(const char* data) override;

    void setCameraInfo(const nx::sdk::CameraInfo& cameraInfo);
    void setDeviceManifest(const QByteArray& manifest);
    void setDriverManifest(const Hanwha::DriverManifest& manifest);

    void setMonitor(HanwhaMetadataMonitor* monitor);

private:
    Hanwha::DriverManifest m_driverManifest;
    QByteArray m_deviceManifest;

    QUrl m_url;
    QString m_model;
    QString m_firmware;
    QAuthenticator m_auth;
    QString m_uniqueId;
    QString m_sharedId;
    int m_channel;

    HanwhaMetadataPlugin* m_plugin = nullptr;
    HanwhaMetadataMonitor* m_monitor = nullptr;
    nx::sdk::metadata::MetadataHandler* m_handler = nullptr;
};

} // namespace plugins
} // namespace mediaserver
} // namespace nx
