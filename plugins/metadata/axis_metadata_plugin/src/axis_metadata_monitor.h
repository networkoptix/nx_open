#pragma once

#include "axis_common.h"

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

namespace nx {
namespace mediaserver {
namespace plugins {

class AxisMetadataManager;

class AxisMetadataMonitor: public QObject
{
    Q_OBJECT

public:
    using Handler = std::function<void(const std::vector<SupportedEventEx>&)>;

    AxisMetadataMonitor(
        AxisMetadataManager* manager,
        const QUrl& resourceUrl,
        const QAuthenticator& auth,
        nx::sdk::metadata::AbstractMetadataHandler* handler);
    virtual ~AxisMetadataMonitor();

    void addRules(const SocketAddress& localAddress, nxpl::NX_GUID* eventTypeList,
        int eventTypeListSize);

    void removeRules();

    HostAddress getLocalIp(const SocketAddress& cameraAddress);
    nx::sdk::Error startMonitoring(nxpl::NX_GUID* eventTypeList, int eventTypeListSize);
    void stopMonitoring();

 private:
    AxisMetadataManager * m_manager;
    const QUrl m_url;
    const QUrl m_endpoint;
    const QAuthenticator m_auth;
    nx::sdk::metadata::AbstractMetadataHandler* m_handler;
    TestHttpServer* m_httpServer;
    mutable QnMutex m_mutex;
};

} // namespace plugins
} // namespace mediaserver
} // namespace nx
