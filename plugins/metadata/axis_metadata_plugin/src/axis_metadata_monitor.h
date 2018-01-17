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
#include <nx/network/http/http_async_client.h>
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
    using Handler = std::function<void(const AxisEventList&)>;

    AxisMetadataMonitor(
        const Axis::DriverManifest& manifest,
        const QUrl& resourceUrl,
        const QAuthenticator& auth);
    virtual ~AxisMetadataMonitor();

    void setRule(const nx::network::SocketAddress& localAddress, nxpl::NX_GUID* eventTypeList,
        int eventTypeListSize);
    nx::network::HostAddress getLocalIp(const nx::network::SocketAddress& cameraAddress);
    nx::sdk::Error startMonitoring(nxpl::NX_GUID* eventTypeList, int eventTypeListSize);
    void stopMonitoring();

    void setManager(AxisMetadataManager* manager);

private:
    const Axis::DriverManifest& m_manifest;
    const QUrl m_url;
    const QUrl m_endpoint;
    const QAuthenticator m_auth;
    nx::network::aio::Timer m_timer;

    static const char* const kWebServerPath;

    mutable QnMutex m_mutex;

    std::vector<int> m_actionIds;
    std::vector<int> m_ruleIds;
    nx::network::http::TestHttpServer* m_httpServer;

    AxisMetadataManager* m_manager = nullptr;
};

} // namespace plugins
} // namespace mediaserver
} // namespace nx
