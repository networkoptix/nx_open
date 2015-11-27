#include "listener.h"

namespace pcp {

Listener::Listener(Events& events)
    : m_events(events)
    , m_socket(SocketFactory::createDatagramSocket())
{
    m_socket->bind(SocketAddress(HostAddress(), CLIENT_PORT));
    m_buffer.reserve(1024);
    readAsync();
}

Listener::~Listener()
{
    m_socket->cancelAsyncIO();
}

void Listener::readAsync()
{
    m_buffer.resize(0);

    using namespace std::placeholders;
    m_socket->readSomeAsync(&m_buffer, std::bind(&Listener::readHandler, this, _1, _2));
}

void Listener::readHandler(SystemError::ErrorCode result, size_t size)
{
    QDataStream stream(m_buffer);

    ResponseHeadeer header;
    stream >> header;
    switch (header.opcode)
    {
        case Opcode::ANNOUNCE:
            m_events.handle(header);
            break;

        case Opcode::MAP: {
            MapMessage message;
            stream >> message;
            m_events.handle(header, message);
            break;
        }

        case Opcode::PEER: {
            PeerMessage message;
            stream >> message;
            m_events.handle(header, message);
            break;
        }

        default:
            // TODO: report error
            break;
    }

    readAsync(); // initiase next read
}

} // namespace pcp
