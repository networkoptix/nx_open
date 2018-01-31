#pragma once

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtCore/QString>
#include <QtNetwork/QAuthenticator>

#include <nx/utils/thread/mutex.h>
#include <plugins/plugin_tools.h>
#include <nx/sdk/metadata/abstract_metadata_manager.h>

#include "common.h"
#include "plugin.h"

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace vca {

class Manager: public nxpt::CommonRefCounter<nx::sdk::metadata::AbstractMetadataManager>
{
public:
    Manager(Plugin* plugin,
        const nx::sdk::ResourceInfo& resourceInfo,
        const Vca::VcaAnalyticsDriverManifest& typedManifest);

    virtual ~Manager();

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;


    void onReceive(SystemError::ErrorCode, size_t);

    virtual nx::sdk::Error startFetchingMetadata(
        nx::sdk::metadata::AbstractMetadataHandler* handler,
        nxpl::NX_GUID* eventTypeList,
        int eventTypeListSize) override;

    virtual nx::sdk::Error stopFetchingMetadata() override;

    virtual const char* capabilitiesManifest(nx::sdk::Error* error) override;

    virtual void freeManifest(const char* data) override;

private:
    QUrl m_url;
    QAuthenticator m_auth;
    Plugin* m_plugin;
    QByteArray m_cameraManifest;
    std::vector<QnUuid> m_eventsToCatch;
    QByteArray m_buffer;
    nx::network::TCPSocket* m_tcpSocket = nullptr;
    nx::sdk::metadata::AbstractMetadataHandler* m_handler = nullptr;
};

} // namespace vca
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
