#include "SessionManager.h"

#include <QtCore/QBuffer>

#include <QtNetwork/QNetworkReply>

#include "utils/network/synchttp.h"

void detail::SessionManagerReplyProcessor::at_replyReceived(QNetworkReply *reply)
{
    emit finished(reply->error(), reply->readAll());

    reply->deleteLater();
    deleteLater();
}


Q_GLOBAL_STATIC_WITH_ARGS(SessionManager, globalSessionManager, (QUrl()))

SessionManager::SessionManager(const QUrl &url, QObject *parent)
    : QObject(parent),
      m_addEndSlash(true)
{
    m_httpClient = new SyncHTTP(url, this);

    QMetaObject::invokeMethod(this, "setupErrorHandler", Qt::QueuedConnection);
}

SessionManager::~SessionManager()
{
}

SessionManager *SessionManager::instance()
{
    return globalSessionManager();
}

void SessionManager::setupErrorHandler()
{
    disconnect(m_httpClient, 0, SessionManager::instance(), 0);

    connect(m_httpClient, SIGNAL(error(int)), SessionManager::instance(), SIGNAL(error(int)));
}

QUrl SessionManager::createApiUrl(const QString &objectName, const QnRequestParamList &params) const
{
    QUrl url;

    QString path = QLatin1String("api/") + objectName;
    if (m_addEndSlash)
        path += QLatin1Char('/');
    url.setPath(path);

    foreach (const QnRequestParam &param, params)
        url.addQueryItem(param.first, param.second);

    return url;
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

    int status = m_httpClient->syncGet(createApiUrl(objectName, params), &buffer);
    if (status != 0)
        m_lastError = formatNetworkError(status) + reply;

    return status;
}

void SessionManager::sendAsyncGetRequest(const QString &objectName, QObject *target, const char *slot)
{
    sendAsyncGetRequest(objectName, QnRequestParamList(), target, slot);
}

void SessionManager::sendAsyncGetRequest(const QString &objectName, const QnRequestParamList &params, QObject *target, const char *slot)
{
    /* We set parent to 'this' so that destroying session manager would stop all requests. */
    detail::SessionManagerReplyProcessor *processor = new detail::SessionManagerReplyProcessor();
    connect(processor, SIGNAL(finished(int, const QByteArray &)), target, slot);

    m_httpClient->asyncGet(createApiUrl(objectName, params), processor, SLOT(at_replyReceived(QNetworkReply *)));
}

void SessionManager::setAddEndSlash(bool value)
{
    m_addEndSlash = value;
}

QByteArray SessionManager::lastError() const
{
    return m_lastError;
}

QByteArray SessionManager::formatNetworkError(int error)
{
    QByteArray errorValue;

    QMetaObject metaObject = QNetworkReply::staticMetaObject;
    const int idx = metaObject.indexOfEnumerator("NetworkError");
    if (idx != -1)
        errorValue = metaObject.enumerator(idx).valueToKey(error);

    return errorValue;
}
