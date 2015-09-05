#ifndef __MULTICAST_HTTP_SERVER_H__
#define __MULTICAST_HTTP_SERVER_H__

#include "core/multicast/multicast_http_fwd.h"
#include "core/multicast/multicast_http_transport.h"
#include <QSet>
#include <memory>
#include "utils/network/http/asynchttpclient.h"

class QThread;

namespace QnMulticast
{
    /** This class executes HTTP requests over multicast transport
    * Client and server could belong different networks
    */
    class HttpServer: public QObject
    {
        Q_OBJECT
    public:
  
        HttpServer(const QUuid& localGuid);
    private:
        void at_gotRequest(const QUuid& requestId, const QUuid& clientId, const Request& request);
    private:
        std::unique_ptr<Transport> m_transport;
        QSet<nx_http::AsyncHttpClientPtr> m_requests;
    };
}


#endif // __MULTICAST_HTTP_SERVER_H__
