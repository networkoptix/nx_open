#pragma once

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtCore/QString>
#include <QtNetwork/QAuthenticator>

#include <nx/utils/thread/mutex.h>

#include <hanwha_metadata_monitor.h>

#include <plugins/plugin_tools.h>
#include <nx/sdk/metadata/abstract_metadata_manager.h>

namespace nx {
namespace mediaserver {
namespace plugins {

class HanwhaMetadataManager:
    public QObject,
    public nxpt::CommonRefCounter<nx::sdk::metadata::AbstractMetadataManager>

{
    Q_OBJECT;
public:
    HanwhaMetadataManager();

    virtual ~HanwhaMetadataManager();

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    virtual nx::sdk::Error startFetchingMetadata(
        nx::sdk::metadata::AbstractMetadataHandler* handler) override;

    virtual nx::sdk::Error stopFetchingMetadata() override;

    virtual const char* capabilitiesManifest(nx::sdk::Error* error) const override;

    void setResourceInfo(const nx::sdk::ResourceInfo& resourceInfo);
    void setDeviceManifest(const QByteArray& manifest);
    void setDriverManifest(const Hanwha::DriverManifest& manifest);

private:
    Hanwha::DriverManifest m_driverManifest;
    QByteArray m_deviceManifest;

    nx::utils::Url m_url;
    QString m_model;
    QString m_firmware;
    QAuthenticator m_auth;

    std::unique_ptr<HanwhaMetadataMonitor> m_monitor;
    nx::sdk::metadata::AbstractMetadataHandler* m_handler = nullptr;
    
};

} // namespace plugins
} // namespace mediaserver
} // namespace nx
