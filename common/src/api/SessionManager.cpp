#include "SessionManager.h"

#include <QtCore/QBuffer>
#include <QtCore/QThread>

#include <QtNetwork/QNetworkReply>

#include "utils/network/synchttp.h"
#include "utils/common/warnings.h"

QAtomicInt SessionManager::m_handle(1);


void detail::SessionManagerReplyProcessor::at_replyReceived(QNetworkReply *reply)
{
    int handle = reply->property("handle").toInt();
    emit finished(reply->error(), reply->readAll(), handle);

    reply->deleteLater();
    deleteLater();
}


Q_GLOBAL_STATIC_WITH_ARGS(SessionManager, globalSessionManager, (QUrl()))

SessionManager::SessionManager(const QUrl &url, QObject *parent)
    : QObject(parent),
      m_addEndSlash(true)
{
    /* We don't add http client to the children list of this object so that it 
     * is not deleted from our destructor. */
    m_httpClient = new SyncHTTP(url); 

    QMetaObject::invokeMethod(this, "setupErrorHandler", Qt::QueuedConnection);
}

SessionManager::~SessionManager()
{
    if(QThread::currentThread() != this->thread())
        qnWarning("Deleting session manager from another thread is dangerous and may lead to unexpected crashes.");

    m_httpClient->deleteLater();
}

SessionManager *SessionManager::instance()
{
    return globalSessionManager();
}

void SessionManager::testConnectionAsync(QObject* receiver, const char *slot)
{
    sendAsyncGetRequest("ping", receiver, slot);
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

int SessionManager::sendGetRequest(const QString &objectName, QByteArray& reply, QByteArray& errorString)
{
    return sendGetRequest(objectName, QnRequestParamList(), reply, errorString);
}

int SessionManager::sendGetRequest(const QString &objectName, const QnRequestParamList &params, QByteArray& reply, QByteArray& errorString)
{
    QBuffer buffer(&reply);
    buffer.open(QIODevice::WriteOnly);

    int status = m_httpClient->syncGet(createApiUrl(objectName, params), &buffer);
    if (status != 0)
    {
        errorString += "\nSessionManager::sendGetRequest(): ";
        errorString += formatNetworkError(status) + reply;
    }

    return status;
}

int SessionManager::sendAsyncGetRequest(const QString &objectName, QObject *target, const char *slot)
{
    return sendAsyncGetRequest(objectName, QnRequestParamList(), target, slot);
}

int SessionManager::sendAsyncGetRequest(const QString &objectName, const QnRequestParamList &params, QObject *target, const char *slot)
{
    /* We set parent to 'this' so that destroying session manager would stop all requests. */
    detail::SessionManagerReplyProcessor *processor = new detail::SessionManagerReplyProcessor();
    connect(processor, SIGNAL(finished(int, const QByteArray &, int)), target, slot);
    QNetworkReply* reply = m_httpClient->asyncGet(createApiUrl(objectName, params), processor, SLOT(at_replyReceived(QNetworkReply *)));
    int handle = m_handle.fetchAndAddAcquire(1);
    reply->setProperty("handle", handle);
    return handle;
}

int SessionManager::sendAsyncPostRequest(const QString &objectName, const QByteArray& data, QObject *target, const char *slot)
{
    return sendAsyncPostRequest(objectName, QnRequestParamList(), data, target, slot);
}

int SessionManager::sendAsyncPostRequest(const QString &objectName, const QnRequestParamList &params, const QByteArray& data, QObject *target, const char *slot)
{
    /* We set parent to 'this' so that destroying session manager would stop all requests. */
    detail::SessionManagerReplyProcessor *processor = new detail::SessionManagerReplyProcessor();
    connect(processor, SIGNAL(finished(int, const QByteArray &, int)), target, slot);
    QNetworkReply* reply = m_httpClient->asyncPost(createApiUrl(objectName, params), data, processor, SLOT(at_replyReceived(QNetworkReply *)));
    int handle = m_handle.fetchAndAddAcquire(1);
    reply->setProperty("handle", handle);
    return handle;
}

void SessionManager::setAddEndSlash(bool value)
{
    m_addEndSlash = value;
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
