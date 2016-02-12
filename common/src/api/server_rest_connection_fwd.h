#pragma once

#include <functional>
#include <QObject>

namespace nx_http {
    class AsyncHttpClientPtr;
};

namespace rest
{
    typedef qint64 Handle;

    // we have to use define instead of 'using' keyword because of deprecated MSVC version
    #define REST_CALLBACK(ResultType) std::function<void (bool, Handle, ResultType)>

    class ServerConnection;
    typedef QSharedPointer<ServerConnection> QnConnectionPtr;
};
