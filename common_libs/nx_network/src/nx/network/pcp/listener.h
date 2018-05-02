#ifndef LISTENER_H
#define LISTENER_H

#include "messaging.h"
#include "nx/utils/thread/long_runnable.h"
#include <nx/network/socket_factory.h>

namespace nx {
namespace network {
namespace pcp {

class NX_NETWORK_API Listener
{
public:
    struct Events
    {
        virtual void handle(const ResponseHeadeer&) {}
        virtual void handle(const ResponseHeadeer&, const MapMessage&) {}
        virtual void handle(const ResponseHeadeer&, const PeerMessage&) {}
    };

    Listener(Events& events);
    ~Listener();

protected:
    void readAsync();
    void readHandler(SystemError::ErrorCode result, size_t size);

private:
    Events& m_events;
    std::unique_ptr<AbstractDatagramSocket> m_socket;
    QByteArray m_buffer;
};

} // namespace pcp
} // namespace network
} // namespace nx

#endif // LISTENER_H
