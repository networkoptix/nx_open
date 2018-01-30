#pragma once

#include <map>
#include <vector>

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtNetwork/QAuthenticator>

#include <nx/network/system_socket.h>

#include <nx/sdk/metadata/abstract_metadata_manager.h>
#include <nx/sdk/metadata/common_detected_event.h>
#include <nx/sdk/metadata/common_event_metadata_packet.h>
#include <nx/network/http/asynchttpclient.h>
#include <nx/network/http/multipart_content_parser.h>
#include <nx/network/http/test_http_server.h>
#include <common/common_module.h>

#include "nx/vca/camera_controller.h"

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace vca {

class Manager;

class Monitor final
{
public:
    Monitor(
        Manager* manager,
        const QUrl& resourceUrl,
        const QAuthenticator& auth,
        nx::sdk::metadata::AbstractMetadataHandler* handler);

    ~Monitor();

    nx::sdk::Error startMonitoring(nxpl::NX_GUID* eventTypeList, int eventTypeListSize);
    void stopMonitoring();
    Manager* manager(){ return m_manager; }

private:
    nx::sdk::Error prepareVca(nx::vca::CameraController& vcaCameraConrtoller);

private:
    Manager* m_manager;
    const QUrl m_url;
    const QAuthenticator m_auth;
    nx::sdk::metadata::AbstractMetadataHandler* m_handler;
    std::vector<QnUuid> m_currentEventIds;
    nx::network::TCPSocket* m_tcpSocket = nullptr;
    QByteArray m_buffer;
};

} // namespace vca
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
