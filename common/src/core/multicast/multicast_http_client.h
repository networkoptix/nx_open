#ifndef __QN_MULTICAST_HTTP_CLIENT_H_
#define __QN_MULTICAST_HTTP_CLIENT_H_

#include "multicast_http_fwd.h"
#include "multicast_http_transport.h"

namespace QnMulticast
{
    /** This class executes HTTP requests over multicast transport
    * Client and server could belong different networks
    */
    class HTTPClient: public QObject
    {
        Q_OBJECT
    public:
  
        HTTPClient(const QUuid& localGuid);
        /** Execute HTTP get request over multicast transport
        * !Returns request handle
        */
        //QUuid doGet(const Request& request, ResponseCallback callback, int timeoutMs = -1);
        /** Execute HTTP post request over multicast transport
        * !Returns request handle
        */
        //QUuid doPost(const Request& request, ResponseCallback callback, int timeoutMs = -1);
        /** Cancel previously started request.  callback function will not called. */
        void cancelRequest(const QUuid& handle);
        /** Set default timeout for requests */
        void setDefaultTimeout(int timeoutMs);
        QUuid execRequest(const Request& request, ResponseCallback callback, int timeoutMs = -1);
    private:
        
    private:
        Transport m_transport;
        QMap<QUuid, QByteArray> m_awaitingResponses;
    };
}

#endif // __QN_MULTICAST_HTTP_CLIENT_H_
