#include "SessionManager.h"
#include <QBuffer>
#include <QNetworkReply>

void detail::SessionManagerReplyProcessor::at_replyReceived(QNetworkReply *reply) {
    emit finished(reply->error(), reply->readAll());
    
    reply->deleteLater();
    deleteLater();
}


SessionManager::SessionManager(const QHostAddress& host, quint16 port, const QAuthenticator& auth, QObject *parent): 
    QObject(parent),
    m_httpClient(host, port, auth),
    m_addEndShash(true)
{
}

SessionManager::~SessionManager() 
{
}

QUrl SessionManager::createApiUrl(const QString &objectName, const QnRequestParamList &params) 
{
    QUrl result;
    result.setPath(QLatin1String("api/") + objectName);

    if (m_addEndShash)
        result.setPath(result.path() +  "/");

    for (int i = 0; i < params.size(); ++i)
        result.addQueryItem(params[i].first, params[i].second);

    return result;
}

int SessionManager::sendGetRequest(const QString &objectName, QByteArray& reply)
{
    return sendGetRequest(objectName, QnRequestParamList(), reply);
}

int SessionManager::sendGetRequest(const QString &objectName, const QnRequestParamList &params, QByteArray& reply)
{
    m_lastError.clear();

    QBuffer buffer(&reply);
    buffer.open(QIODevice::WriteOnly);

    int status = m_httpClient.syncGet(createApiUrl(objectName, params).toEncoded(), &buffer);
    if (status != 0)
        m_lastError = formatNetworkError(status) + reply;

    return status;
}

void SessionManager::sendAsyncGetRequest(const QString &objectName, const QnRequestParamList &params, QObject *target, const char *slot)
{
    /* We set parent to 'this' so that destroying session manager would stop all requests. */
    detail::SessionManagerReplyProcessor *processor = new detail::SessionManagerReplyProcessor();
    connect(processor, SIGNAL(finished(int, const QByteArray &)), target, slot);

    m_httpClient.asyncGet(createApiUrl(objectName, params).toEncoded(), processor, SLOT(at_replyReceived(QNetworkReply *)));
}

void SessionManager::sendAsyncGetRequest(const QString &objectName, QObject *target, const char *slot)
{
    sendAsyncGetRequest(objectName, QnRequestParamList(), target, slot);
}

void SessionManager::setAddEndShash(bool value)
{
    m_addEndShash = value;
}

QByteArray SessionManager::getLastError()
{
    return m_lastError;
}

QByteArray SessionManager::formatNetworkError(int error)
{
    QByteArray errorValue;

    QMetaObject meta = QNetworkReply::staticMetaObject;
    for (int i=0; i < meta.enumeratorCount(); ++i) {
        QMetaEnum m = meta.enumerator(i);
        if (m.name() == QLatin1String("NetworkError")) {
            errorValue = m.valueToKey(error);
            break;
        }
    }

    return errorValue;
}
