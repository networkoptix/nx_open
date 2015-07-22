#ifndef SENDER_H
#define SENDER_H

#include "messaging.h"
#include "utils/network/socket_factory.h"

namespace pcp {

class Sender
{
public:
    Sender(const HostAddress& server);
    ~Sender();

    struct EmptyMessage {};

    template <typename Message = EmptyMessage>
    void send(const RequestHeader& request, const Message& message = Message())
    {
        const auto buffer = std::make_shared<QByteArray>();
        QDataStream stream(buffer.get(), QIODevice::WriteOnly);

        stream << request << message;
        send(buffer);
    }

private:
    void send(std::shared_ptr<QByteArray> request);

private:
    std::unique_ptr<AbstractDatagramSocket> m_socket;
};

inline
QDataStream& operator<<(QDataStream& s, const Sender::EmptyMessage&) { return s; }

} // namespace pcp

#endif // SENDER_H
