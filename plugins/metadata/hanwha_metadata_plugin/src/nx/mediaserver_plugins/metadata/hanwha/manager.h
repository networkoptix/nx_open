#pragma once

#if defined(ENABLE_HANWHA)

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtNetwork/QAuthenticator>

#include <plugins/plugin_tools.h>
#include <nx/sdk/metadata/abstract_metadata_manager.h>

#include "plugin.h"
#include "metadata_monitor.h"

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace hanwha {

class Manager:
    public QObject,
    public nxpt::CommonRefCounter<nx::sdk::metadata::AbstractMetadataManager>
{
    Q_OBJECT

public:
    Manager(Plugin* plugin);
    virtual ~Manager();

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    virtual nx::sdk::Error setHandler(
        nx::sdk::metadata::AbstractMetadataHandler* handler) override;
    virtual nx::sdk::Error startFetchingMetadata(
        nxpl::NX_GUID* eventTypeList,
        int eventTypeListSize) override;

    virtual nx::sdk::Error stopFetchingMetadata() override;

    virtual const char* capabilitiesManifest(nx::sdk::Error* error) const override;

    void setResourceInfo(const nx::sdk::ResourceInfo& resourceInfo);
    void setDeviceManifest(const QByteArray& manifest);
    void setDriverManifest(const Hanwha::DriverManifest& manifest);

    void setMonitor(MetadataMonitor* monitor);

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
    nx::sdk::metadata::AbstractMetadataHandler* m_handler = nullptr;
};

} // namespace hanwha
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx

#endif // defined(ENABLE_HANWHA)
