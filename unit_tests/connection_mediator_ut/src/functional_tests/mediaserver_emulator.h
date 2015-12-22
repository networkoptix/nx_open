/**********************************************************
* Dec 22, 2015
* akolesnikov
***********************************************************/

#ifndef MEDIASERVER_EMULATOR_H
#define MEDIASERVER_EMULATOR_H

#include <cloud_data_provider.h>
#include <nx/network/cloud_connectivity/mediator_connector.h>
#include <nx/network/http/server/http_message_dispatcher.h>
#include <nx/network/http/server/http_stream_socket_server.h>
#include <nx/network/socket.h>
#include "mediator_connector.h"


namespace nx {
namespace hpm {

class MediaServerEmulator
{
public:
    MediaServerEmulator(
        const SocketAddress& mediatorEndpoint,
        AbstractCloudDataProvider::System systemData);
    virtual ~MediaServerEmulator();

    //!Attaches to a local port and registers on mediator
    bool start();

private:
    MediatorConnector m_mediatorConnector;
    nx_http::MessageDispatcher m_httpMessageDispatcher;
    nx_http::HttpStreamSocketServer m_httpServer;
    AbstractCloudDataProvider::System m_systemData;
    nx::String m_serverID;
    std::shared_ptr<nx::cc::MediatorSystemConnection> m_systemClient;
};

}   //hpm
}   //nx

#endif  //MEDIASERVER_EMULATOR_H
