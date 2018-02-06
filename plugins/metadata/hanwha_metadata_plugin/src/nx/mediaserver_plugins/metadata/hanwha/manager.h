#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtNetwork/QAuthenticator>

#include <plugins/plugin_tools.h>
#include <nx/sdk/metadata/camera_manager.h>

#include "plugin.h"
#include "metadata_monitor.h"

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace hanwha {

class Manager:
    public QObject,
    public nxpt::CommonRefCounter<nx::sdk::metadata::CameraManager>
{
    Q_OBJECT

public:
    Manager(Plugin* plugin);
    virtual ~Manager();

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    virtual nx::sdk::Error setHandler(
        nx::sdk::metadata::MetadataHandler* handler) override;

    virtual nx::sdk::Error startFetchingMetadata(
        nxpl::NX_GUID* typeList,
        int typeListSize) override;

    virtual nx::sdk::Error stopFetchingMetadata() override;

    virtual const char* capabilitiesManifest(nx::sdk::Error* error) override;

    virtual void freeManifest(const char* data) override;

    void setCameraInfo(const nx::sdk::CameraInfo& cameraInfo);
    void setDeviceManifest(const QByteArray& manifest);
    void setDriverManifest(const Hanwha::DriverManifest& manifest);

    void setMonitor(MetadataMonitor* monitor);

    virtual void setDeclaredSettings(const nxpl::Setting* settings, int count) override;

private:
    Hanwha::DriverManifest m_driverManifest;
    QByteArray m_deviceManifest;

    nx::utils::Url m_url;
    QString m_model;
    QString m_firmware;
    QAuthenticator m_auth;
    QString m_uniqueId;
    QString m_sharedId;
    int m_channel = 0;

    Plugin* const m_plugin;
    MetadataMonitor* m_monitor = nullptr;
    nx::sdk::metadata::MetadataHandler* m_handler = nullptr;
};

} // namespace hanwha
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
