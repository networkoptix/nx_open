
#pragma once


namespace nx {
namespace network {
namespace aio {

enum EventType
{
    etNone = 0,
    etRead = 1,
    etWrite = 2,
    //!Error occured on socket. Output only event. To get socket error code use Socket::getLastError
    etError = 4,
    //!Used for periodic operations and for socket timers
    etTimedOut = 8,
    etReadTimedOut = etRead | etTimedOut,
    etWriteTimedOut = etWrite | etTimedOut,
    etMax = 9
};

const char* toString(EventType eventType);

}   //aio
}   //network
}   //nx
