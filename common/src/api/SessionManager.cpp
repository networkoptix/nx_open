#include <QTextStream>

#include "SessionManager.h"

void detail::SessionManagerReplyForwarder::at_replyReceived(QNetworkReply *reply) {
    QScopedPointer<QNetworkReply> guard(reply);

    

}


SessionManager::SessionManager(const QHostAddress& host, quint16 port, const QAuthenticator& auth)
    : m_httpClient(host, port, auth)
{
    m_addEndShash = true;
}

SessionManager::~SessionManager()
{
}

int SessionManager::sendGetRequest(const QString &objectName, QByteArray& reply)
{
    return sendGetRequest(objectName, QnRequestParamList(), reply);
}

int SessionManager::sendGetRequest(const QString &objectName, const QnRequestParamList &params, QByteArray& reply)
{
    m_lastError.clear();

    QUrl url;
    url.setPath(objectName);

    if (m_addEndShash)
        url.setPath(url.path() +  "/");

    for (int i = 0; i < params.size(); ++i)
    {
        url.addQueryItem(params[i].first, params[i].second);
    }

    QBuffer buffer(&reply);
    buffer.open(QIODevice::WriteOnly);

    int status = m_httpClient.syncGet(QByteArray("api/") + url.toEncoded(), &buffer);
    if (status != 0)
        m_lastError = formatNetworkError(status) + reply;

    return status;
}

void SessionManager::sendGetRequest(const QString &objectName, const QnRequestParamList &params, QObject *target, const char *slot)
{

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
