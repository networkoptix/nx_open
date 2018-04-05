#ifndef __MULTICAST_HTTP_SERVER_H__
#define __MULTICAST_HTTP_SERVER_H__

#include "core/multicast/multicast_http_fwd.h"
#include "core/multicast/multicast_http_transport.h"
#include <memory>
#include <nx/network/http/http_async_client.h>

class QThread;
class QnTcpListener;

namespace QnMulticast
{
    /** This class executes HTTP requests over multicast transport
    * Client and server could belong different networks
    */
    class HttpServer: public QObject
    {
        Q_OBJECT
    public:

        HttpServer(const QUuid& localGuid, QnTcpListener* tcpListener);
        virtual ~HttpServer() override;
    private:
        void at_gotRequest(const QUuid& requestId, const QUuid& clientId, const Request& request);
    private:
        mutable QnMutex m_mutex;
        std::unique_ptr<Transport> m_transport;
        std::map<QnUuid, std::unique_ptr<nx::network::http::AsyncClient>> m_requests;
        QnTcpListener* m_tcpListener;
    };
}


#endif // __MULTICAST_HTTP_SERVER_H__
