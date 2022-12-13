// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/socket_factory.h>
#include <nx/utils/thread/long_runnable.h>

#include "messaging.h"

namespace nx::network::pcp {

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
    nx::Buffer m_buffer;
};

} // namespace nx::network::pcp
