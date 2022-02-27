// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "websocket_common_types.h"

namespace nx::network::websocket {

std::string toString(Error error)
{
    switch (error)
    {
        case Error::noError:
            return "noError";
        case Error::noMaskBit:
            return "noMaskBit";
        case Error::maskIsZero:
            return "maskIsZero";
        case Error::handshakeError:
            return "handshakeError";
        case Error::connectionAbort:
            return "connectionAbort";
        case Error::timedOut:
            return "timedOut";
    }

    return "unknown";
}

} // namespace nx::network::websocket
