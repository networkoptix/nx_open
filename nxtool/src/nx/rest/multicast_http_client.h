#pragma once

#include <memory>
#include <QElapsedTimer>

#include "multicast_http_fwd.h"

namespace nx {
namespace rest {

class MulticastHttpTransport;

/**
    * Executes HTTP requests over multicast transport. Client and server may
    * belong to different networks.
    */
class MulticastHttpClient
{
public:
    /** 
        * !param userAgent client side human-readable name. It must be provided for server-side audit trails purpose
        * !param localGuid optional parameter to identify client side. Auto-generated if empty.
        */
    explicit MulticastHttpClient(const QString& userAgent, const QUuid& localGuid = QUuid());
    virtual ~MulticastHttpClient();

    /** Cancel previously started request. Callback function will not be called. */
    void cancelRequest(const QUuid& requestId);

    /**
        * Execute HTTP request over multicast transport.
        * !Returns request handle
        */
    QUuid execRequest(const multicastHttp::Request& request, multicastHttp::ResponseCallback callback,
        int timeoutMs);
private:
    struct AuthInfo
    {
        QString realm;
        qint64 nonce;
        QElapsedTimer timer; //< UTC usec timer could be used as nonce for our server. We are using elapsed timer for relative offset from first server UTC answer
    };

    std::unique_ptr<MulticastHttpTransport> m_transport;
    QMap<QUuid, AuthInfo> m_authByServer;  // cached auth info by server (realm, nonce)
    QMap<QUuid, QUuid> m_requestsPairs;    // map first and second requests ID's
    QString m_userAgent;
private:
    multicastHttp::Request updateRequest(const multicastHttp::Request& srcRequest);
    QByteArray createHttpQueryAuthParam(const QString& userName,
        const QString& password,
        const QString& realm,
        const QByteArray& method,
        QByteArray nonce);
    QByteArray createUserPasswordDigest(const QString& userName, const QString& password, const QString& realm);
    void updateAuthParams(const QUuid& serverId, const multicastHttp::Response& response);
};

} // namespace rest
} // namespace nx
