#include "multicast_http_client.h"
#include <QCryptographicHash>
#include "network/authutil.h"

namespace QnMulticast
{

static const int HTTP_NOT_AUTHORIZED = 401;

HTTPClient::HTTPClient(const QUuid& localGuid):
    m_transport(localGuid),
    m_defaultTimeoutMs(-1)
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
    const QByteArray& ha1 = createUserPasswordDigest( userName, password, realm );

    //calculating "HA2"
    QCryptographicHash md5Hash( QCryptographicHash::Md5 );
    md5Hash.addData( method );
    md5Hash.addData( ":" );
    const QByteArray nedoHa2 = md5Hash.result().toHex();

    //calculating auth digest
    md5Hash.reset();
    md5Hash.addData( ha1 );
    md5Hash.addData( ":" );
    md5Hash.addData( nonce );
    md5Hash.addData( ":" );
    md5Hash.addData( nedoHa2 );
    const QByteArray& authDigest = md5Hash.result().toHex();

    return (userName.toUtf8() + ":" + nonce + ":" + authDigest).toBase64();
}


Request HTTPClient::addAuthHeader(const Request& srcRequest)
{
    AuthInfo& authInfo = m_authByServer[srcRequest.serverId];
    if (authInfo.realm.isEmpty())
        return srcRequest; // realm isn't known yet.

    Request request(srcRequest);
    QByteArray nonce = QByteArray::number(authInfo.nonce + authInfo.timer.elapsed() * 1000ll, 16);
    QString auth = QLatin1String(createHttpQueryAuthParam(request.auth.user(), request.auth.password(), authInfo.realm, srcRequest.method.toUtf8(), nonce));
    QUrlQuery query(request.url.query());
    query.addQueryItem(lit("auth"), auth);
    request.url.setQuery(query);

    return request;
}

void HTTPClient::updateAuthParams(const QUuid& serverId, const Response& response)
{
    for (const auto& header: response.httpHeaders)
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
    if (timeoutMs == -1)
        timeoutMs = m_defaultTimeoutMs;
    auto requestId = m_transport.addRequest(addAuthHeader(request), [request, callback, timeoutMs, this](const QUuid& requestId, ErrCode errCode, const Response& response) 
    {
        if (response.httpResult == HTTP_NOT_AUTHORIZED) {
            updateAuthParams(request.serverId, response);
            auto requestId2 = m_transport.addRequest(addAuthHeader(request), [request, callback, timeoutMs, this](const QUuid& requestId, ErrCode errCode, const Response& response)
            {
                for (auto itr = m_requestsPairs.begin(); itr != m_requestsPairs.end(); ++itr) {
                    if (itr.value() == requestId) {
                        m_requestsPairs.erase(itr);
                        break;
                    }
                }
                m_requestsPairs.remove(requestId);
                callback(requestId, errCode, response);
            },
            timeoutMs);
            m_requestsPairs.insert(requestId, requestId2);
        }
        else
            callback(requestId, errCode, response);
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
    m_defaultTimeoutMs = timeoutMs;
}

}
