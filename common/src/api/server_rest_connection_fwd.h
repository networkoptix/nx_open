#pragma once

#include <functional>
#include <QObject>

#include <nx/vms/event//event_fwd.h>

namespace nx {
namespace network {
namespace http {

class AsyncHttpClientPtr;

} // namespace nx
} // namespace network
} // namespace http

namespace rest
{
#ifdef __arm__
    //#ak ISD Jaguar lacks kernel support for atomic int64
    typedef int Handle;
#else
    typedef qint64 Handle;
#endif

    template<typename ResultType>
    using Callback = std::function<void (bool, Handle, ResultType)>;

    class ServerConnection;
    using QnConnectionPtr = QSharedPointer<ServerConnection>;
};
