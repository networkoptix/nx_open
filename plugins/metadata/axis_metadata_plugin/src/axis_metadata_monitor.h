#pragma once

#include <map>
#include <vector>

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtNetwork/QAuthenticator>

#include <nx/sdk/metadata/abstract_metadata_manager.h>
#include <nx/sdk/metadata/common_detected_event.h>
#include <nx/sdk/metadata/common_event_metadata_packet.h>
#include <nx/network/http/asynchttpclient.h>
#include <nx/network/http/multipart_content_parser.h>
#include <nx/network/http/test_http_server.h>
#include <common/common_module.h>

#include "identified_supported_event.h"

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace axis {

class Manager;

class Monitor: public QObject
{
    Q_OBJECT

public:
    using Handler = std::function<void(const std::vector<IdentifiedSupportedEvent>&)>;

    Monitor(
        Manager* manager,
        const QUrl& resourceUrl,
        const QAuthenticator& auth,
        nx::sdk::metadata::AbstractMetadataHandler* handler);
    virtual ~Monitor();

    void addRules(const SocketAddress& localAddress, nxpl::NX_GUID* eventTypeList,
        int eventTypeListSize);

    void removeRules();

    HostAddress getLocalIp(const SocketAddress& cameraAddress);
    nx::sdk::Error startMonitoring(nxpl::NX_GUID* eventTypeList, int eventTypeListSize);
    void stopMonitoring();

 private:
    Manager * m_manager;
    const QUrl m_url;
    const QUrl m_endpoint;
    const QAuthenticator m_auth;
    nx::sdk::metadata::AbstractMetadataHandler* m_handler;
    TestHttpServer* m_httpServer;
    mutable QnMutex m_mutex;
};

} // axis
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
