/**********************************************************
* Dec 28, 2015
* akolesnikov
***********************************************************/

#include "udp_server.h"

#include <memory>

#include <nx/utils/log/log.h>

#include "abstract_server_connection.h"
#include "message_dispatcher.h"
#include "udp_message_response_sender.h"


namespace nx {
namespace stun {

static const std::chrono::seconds kRetryReadAfterFailureTimeout(1);

UDPServer::UDPServer(const MessageDispatcher& dispatcher)
:
    base_type(this),
    m_dispatcher(dispatcher)
{
}

UDPServer::~UDPServer()
{
    pleaseStopSync();
}

bool UDPServer::listen()
{
    startReceivingMessages();
    return true;
}

void UDPServer::messageReceived(SocketAddress sourceAddress, Message mesage)
{
    m_dispatcher.dispatchRequest(
        std::make_shared<UDPMessageResponseSender>(this, std::move(sourceAddress)),
        std::move(mesage));
}

void UDPServer::ioFailure(SystemError::ErrorCode)
{
    //TODO #ak
}

}   //stun
}   //nx
