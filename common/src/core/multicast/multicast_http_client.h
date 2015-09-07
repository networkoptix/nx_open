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
        /** Cancel previously started request.  callback function will not called. */
        void cancelRequest(const QUuid& requestId);

        /** Set default timeout for requests */
        void setDefaultTimeout(int timeoutMs);

        /** Execute HTTP request over multicast transport
        * !Returns request handle
        */
        QUuid execRequest(const Request& request, ResponseCallback callback, int timeoutMs = -1);
    private:
        struct AuthInfo
        {
            QString realm;
            qint64 nonce;
            QElapsedTimer timer;
        };

        Transport m_transport;
        int m_defaultTimeoutMs;
        QMap<QUuid, AuthInfo> m_authByServer;
        QMap<QUuid, QUuid> m_requestsPairs; // map first and second requests ID's
    private:
        Request addAuthHeader(const Request& srcRequest);
        QByteArray createHttpQueryAuthParam(const QString& userName,
            const QString& password,
            const QString& realm,
            const QByteArray& method,
            QByteArray nonce);
        QByteArray createUserPasswordDigest(const QString& userName, const QString& password, const QString& realm);
        void updateAuthParams(const QUuid& serverId, const Response& response);
    };
}

#endif // __QN_MULTICAST_HTTP_CLIENT_H_
