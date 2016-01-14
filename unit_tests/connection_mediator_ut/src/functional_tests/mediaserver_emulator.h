/**********************************************************
* Dec 22, 2015
* akolesnikov
***********************************************************/

#ifndef MEDIASERVER_EMULATOR_H
#define MEDIASERVER_EMULATOR_H

#include <cloud_data_provider.h>
#include <nx/network/cloud/mediator_connector.h>
#include <nx/network/http/server/http_message_dispatcher.h>
#include <nx/network/http/server/http_stream_socket_server.h>
#include <nx/network/socket.h>
#include "mediator_connector.h"


namespace nx {
namespace hpm {

class MediaServerEmulator
{
public:
    /**
        \param serverName If empty, name is generated
    */
    MediaServerEmulator(
        const SocketAddress& mediatorEndpoint,
        AbstractCloudDataProvider::System systemData,
        nx::String serverName = nx::String());
    virtual ~MediaServerEmulator();

    /** Attaches to a local port and registers on mediator */
    bool start();
    nx::String serverID() const;
    /** Server endpoint */
    SocketAddress endpoint() const;

    nx::hpm::api::ResultCode listen() const;

    /** Address of connection to mediator */
    SocketAddress mediatorConnectionLocalAddress() const;

private:
    MediatorConnector m_mediatorConnector;
    nx_http::MessageDispatcher m_httpMessageDispatcher;
    nx_http::HttpStreamSocketServer m_httpServer;
    AbstractCloudDataProvider::System m_systemData;
    nx::String m_serverID;
    std::shared_ptr<nx::network::cloud::MediatorSystemConnection> m_systemClient;

    MediaServerEmulator(const MediaServerEmulator&);
    MediaServerEmulator& operator=(const MediaServerEmulator&);
};

}   //hpm
}   //nx

#endif  //MEDIASERVER_EMULATOR_H
