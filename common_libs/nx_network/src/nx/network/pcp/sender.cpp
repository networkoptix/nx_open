#include "sender.h"

namespace pcp {

Sender::Sender(const HostAddress& server)
    : m_socket(SocketFactory::createDatagramSocket())
{
    m_socket->setDestAddr(SocketAddress(server, SERVER_PORT));
}

Sender::~Sender()
{
    m_socket->cancelAsyncIO();
}

void Sender::send(std::shared_ptr<QByteArray> request)
{
    m_socket->sendAsync(*request, [request](SystemError::ErrorCode result, size_t)
    {
        static_cast<void>(request);
        if (result == SystemError::noError)
            qDebug() << "Sent:" << request->toHex();
        else
            qDebug() << "Error:" << SystemError::toString(result);
    });
}

} // namespace pcp
