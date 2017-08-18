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

    void setUrl(const QString& url);

    void setModel(const QString& model);

    void setFirmware(const QString& firmware);

    void setAuth(const QAuthenticator& auth);

    void setCapabilitiesManifest(const QByteArray& manifest);

private:
    mutable QnMutex m_mutex;
    QByteArray m_manifest;

    QUrl m_url;
    QString m_model;
    QString m_firmware;
    QAuthenticator m_auth;

    std::unique_ptr<HanwhaMetadataMonitor> m_monitor;
    nx::sdk::metadata::AbstractMetadataHandler* m_handler = nullptr;
    
};

} // namespace plugins
} // namespace mediaserver
} // namespace nx
