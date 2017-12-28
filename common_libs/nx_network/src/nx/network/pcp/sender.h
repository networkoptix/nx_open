#ifndef SENDER_H
#define SENDER_H

#include "messaging.h"
#include <nx/network/socket_factory.h>

namespace nx {
namespace network {
namespace pcp {

class NX_NETWORK_API Sender
{
public:
    Sender(const HostAddress& server);
    ~Sender();

    struct EmptyMessage {};

    template <typename Message>
    void send(const RequestHeader& request, const Message& message = Message())
    {
        auto buffer = std::make_shared<QByteArray>();
        QDataStream stream(buffer.get(), QIODevice::WriteOnly);

        stream << request << message;
        send(std::move(buffer));
    }

private:
    void send(std::shared_ptr<QByteArray> request);

private:
    std::unique_ptr<AbstractDatagramSocket> m_socket;
};

inline
QDataStream& operator<<(QDataStream& s, const Sender::EmptyMessage&)
{ return s; }

} // namespace pcp
} // namespace network
} // namespace nx

#endif // SENDER_H
