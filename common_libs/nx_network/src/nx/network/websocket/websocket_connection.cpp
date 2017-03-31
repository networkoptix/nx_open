#include <nx/network/websocket/websocket_connection.h>

namespace nx {
namespace network {
namespace websocket {

Connection::Connection(
    StreamConnectionHolder<Connection>* socketServer,
    std::unique_ptr<AbstractStreamSocket> sock,
    int version,
    bool isSecure,
    bool isServer)
    :
    BaseType(socketServer, std::move(sock)),
    m_conMessageManager(new ConnectionMessageManagerType<Message>)
{
    switch (version)
    {
    case 0:
        m_processor.reset(
            new ProcessorHybi00Type(
                isSecure,
                isServer,                
                m_conMessageManager));
        break;
    case 7:
        m_processor.reset(
            new ProcessorHybi07Type(
                isSecure,
                isServer,
                m_conMessageManager,
                m_randomGen));
        break;
    case 8:
        m_processor.reset(
            new ProcessorHybi08Type(
                isSecure,
                isServer,
                m_conMessageManager,
                m_randomGen));
        break;
    case 13:
        m_processor.reset(
            new ProcessorHybi13Type(
                isSecure,
                isServer,
                m_conMessageManager,
                m_randomGen));
        break;
    }
}

void Connection::pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler)
{

}

void Connection::processMessage(Message&& request)
{
}

}
}
}