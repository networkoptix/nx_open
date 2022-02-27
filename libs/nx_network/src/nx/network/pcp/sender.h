// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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
        QByteArray buffer;
        QDataStream stream(&buffer, QIODevice::WriteOnly);
        stream << request << message;

        send(std::make_shared<nx::Buffer>(buffer.toStdString()));
    }

private:
    void send(std::shared_ptr<nx::Buffer> request);

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
