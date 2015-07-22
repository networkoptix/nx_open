#ifndef LISTENER_H
#define LISTENER_H

#include "messaging.h"
#include "utils/common/long_runnable.h"
#include "utils/network/socket_factory.h"

namespace pcp {

class Listener
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

}

#endif // LISTENER_H
