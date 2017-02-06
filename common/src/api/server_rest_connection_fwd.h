#pragma once

#include <functional>
#include <QObject>

namespace nx_http {
    class AsyncHttpClientPtr;
};

namespace rest
{
#ifdef __arm__
    //#ak ISD Jaguar lacks kernel support for atomic int64
    typedef int Handle;
#else
    typedef qint64 Handle;
#endif



    // we have to use define instead of 'using' keyword because of deprecated MSVC version
    #define REST_CALLBACK(ResultType) std::function<void (bool, Handle, ResultType)>

    class ServerConnection;
    typedef QSharedPointer<ServerConnection> QnConnectionPtr;
};
