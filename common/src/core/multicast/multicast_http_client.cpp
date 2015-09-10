#include "multicast_http_client.h"
#include <QCryptographicHash>
#include "network/authutil.h"

namespace QnMulticast
{

static const int HTTP_NOT_AUTHORIZED = 401;

HTTPClient::HTTPClient(const QString& userAgent, const QUuid& localGuid):
    m_transport(localGuid.isNull() ? QUuid::createUuid() : localGuid),
    m_defaultTimeoutMs(NO_TIMEOUT),
    m_userAgent(userAgent)
{

}

QByteArray HTTPClient::createUserPasswordDigest(
    const QString& userName,
    const QString& password,
    const QString& realm )
{
    QCryptographicHash md5(QCryptographicHash::Md5);
    md5.addData(QString(lit("%1:%2:%3")).arg(userName, realm, password).toLatin1());
    return md5.result().toHex();
}

QByteArray HTTPClient::createHttpQueryAuthParam(
    const QString& userName,
    const QString& password,
    const QString& realm,
    const QByteArray& method,
    QByteArray nonce )
{
    //calculating user digest
    const QByteArray& ha1 = createUserPasswordDigest( userName.toLower(), password, realm );

    //calculating "HA2"
    QCryptographicHash md5Hash( QCryptographicHash::Md5 );
    md5Hash.addData( method );
    md5Hash.addData( ":" );
    const QByteArray ha2Light = md5Hash.result().toHex();

    //calculating auth digest
    md5Hash.reset();
    md5Hash.addData( ha1 );
    md5Hash.addData( ":" );
    md5Hash.addData( nonce );
    md5Hash.addData( ":" );
    md5Hash.addData( ha2Light );
    const QByteArray& authDigest = md5Hash.result().toHex();

    return (userName.toUtf8() + ":" + nonce + ":" + authDigest).toBase64();
}


Request HTTPClient::updateRequest(const Request& srcRequest)
{
    Request request(srcRequest);

    // add user agent header

    Header userAgent;
    userAgent.first = lit("User-Agent");
    userAgent.second = m_userAgent;
    request.headers << userAgent;

    // add user IP info header
    
    Header localAddress;
    localAddress.first = lit("X-User-Host");
    localAddress.second = m_transport.localAddress();
    request.headers << localAddress;

    // add auth params to url query

    AuthInfo& authInfo = m_authByServer[srcRequest.serverId];
    if (authInfo.realm.isEmpty())
        return request; // realm isn't known yet.

    QByteArray nonce = QByteArray::number(authInfo.nonce + authInfo.timer.elapsed() * 1000ll, 16);
    QString auth = QLatin1String(createHttpQueryAuthParam(request.auth.user(), request.auth.password(), authInfo.realm, srcRequest.method.toUtf8(), nonce));
    QUrlQuery query(request.url.query());
    query.addQueryItem(lit("auth"), auth);
    request.url.setQuery(query);

    return request;
}

void HTTPClient::updateAuthParams(const QUuid& serverId, const Response& response)
{
    for (const auto& header: response.headers)
    {
        if (header.first == lit("WWW-Authenticate"))
        {
            auto params = parseAuthData(header.second.mid(header.second.indexOf(L' ')).toUtf8(), ',');
            AuthInfo& authInfo = m_authByServer[serverId];
            authInfo.realm = QLatin1String(params.value("realm"));
            authInfo.nonce = params.value("nonce").toLongLong(0, 16);
            authInfo.timer.restart();
        }
    }
}

QUuid HTTPClient::execRequest(const Request& request, ResponseCallback callback, int timeoutMs)
{
    if (timeoutMs == NO_TIMEOUT)
        timeoutMs = m_defaultTimeoutMs;
    auto requestId = m_transport.addRequest(updateRequest(request), [request, callback, timeoutMs, this](const QUuid& requestId, ErrCode errCode, const Response& response) 
    {
        if (response.httpResult == HTTP_NOT_AUTHORIZED) {
            updateAuthParams(request.serverId, response);
            auto requestId2 = m_transport.addRequest(updateRequest(request), [request, callback, timeoutMs, this](const QUuid& requestId2, ErrCode errCode, const Response& response)
            {
                QUuid primaryRequestId = m_requestsPairs.key(requestId2, requestId2);
                m_requestsPairs.remove(primaryRequestId);
                if (callback)
                    callback(primaryRequestId, errCode, response);
            },
            timeoutMs);
            m_requestsPairs.insert(requestId, requestId2);
        }
        else {
            if (callback)
                callback(requestId, errCode, response);
        }
    }, 
    timeoutMs);
    return requestId;
}

void HTTPClient::cancelRequest(const QUuid& requestId)
{
    m_transport.cancelRequest(requestId);
    m_transport.cancelRequest(m_requestsPairs.value(requestId));
    m_requestsPairs.remove(requestId);
}

void HTTPClient::setDefaultTimeout(int timeoutMs)
{
    Q_ASSERT(timeoutMs > 0);
    m_defaultTimeoutMs = timeoutMs;
}

}
