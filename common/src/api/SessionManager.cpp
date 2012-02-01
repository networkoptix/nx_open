#include "SessionManager.h"

#include <QtCore/QBuffer>
#include <QtCore/QThread>

#include <QtNetwork/QNetworkReply>

#include "utils/common/warnings.h"

SessionManager* SessionManager::m_instance = 0;

void SessionManagerReplyProcessor::at_replyReceived(QNetworkReply *reply)
{
    QObject * target =reply->property("target").value<QObject*>();
    QByteArray slot = reply->property("slot").toByteArray();

    QMetaObject::invokeMethod(target, slot, Qt::QueuedConnection, Q_ARG(int, reply->error()), Q_ARG(const QByteArray&, reply->readAll()));

    reply->deleteLater();
}


// Q_GLOBAL_STATIC_WITH_ARGS(SessionManager, globalSessionManager, (QUrl()))

SessionManager::SessionManager()
    : m_accessManager(this),
    m_replyProcessor(this)

{
    qRegisterMetaType<QnRequestParamList>("QnRequestParamList");

    connect(&m_accessManager, SIGNAL(finished(QNetworkReply*)), &m_replyProcessor, SLOT(at_replyReceived(QNetworkReply*)));
    connect(this, SIGNAL(asyncGetRequest(QUrl, QString,QnRequestParamList,QObject*,const char*)), this, SLOT(doSendAsyncGetRequest(QUrl, QString,QnRequestParamList,QObject*,const char*)));
    connect(this, SIGNAL(asyncPostRequest(QUrl, QString,QnRequestParamList,QByteArray,QObject*,const char*)), this, SLOT(doSendAsyncPostRequest(QUrl, QString,QnRequestParamList,QByteArray,QObject*,const char*)));
}

SessionManager::~SessionManager()
{
    if(QThread::currentThread() != this->thread())
        qnWarning("Deleting session manager from another thread is dangerous and may lead to unexpected crashes.");
}

SessionManager *SessionManager::instance()
{
    if (m_instance)
        return m_instance;

    m_instance = new SessionManager();
    return m_instance; // globalSessionManager();
}

void SessionManager::testConnectionAsync(const QUrl& url, QObject* receiver, const char *slot)
{
    sendAsyncGetRequest(url, "ping", receiver, slot);
}

QUrl SessionManager::createApiUrl(const QUrl& baseUrl, const QString &objectName, const QnRequestParamList &params) const
{
    QUrl url(baseUrl);

    QString path = QLatin1String("api/") + objectName + QLatin1Char('/');
    url.setPath(path);

    foreach (const QnRequestParam &param, params)
        url.addQueryItem(param.first, param.second);

    return url;
}

int SessionManager::sendPostRequest(const QUrl& url, const QString &objectName, const QByteArray& data, QByteArray &reply, QByteArray& errorString)
{
    return sendPostRequest(url, objectName, QnRequestParamList(), data, reply, errorString);
}

void SyncRequestProcessor::finished(int status, const QByteArray& reply)
{
    m_status = status;
    m_reply = reply;

    m_finished = true;

    QMutexLocker locker(&m_mutex);
    m_condition.wakeOne();
}

int SyncRequestProcessor::wait(QByteArray& reply, QByteArray& errorString)
{
    QMutexLocker locker(&m_mutex);
    while (!m_finished) {
        m_condition.wait(&m_mutex);
    }

    reply = m_reply;
    return m_status;
}

int SessionManager::sendPostRequest(const QUrl& url, const QString &objectName, const QnRequestParamList &params, const QByteArray& data, QByteArray &reply, QByteArray& errorString)
{
    SyncRequestProcessor syncProcessor(this);
    syncProcessor.moveToThread(this->thread());
    sendAsyncPostRequest(url, objectName, params, data, &syncProcessor, "finished");
    return syncProcessor.wait(reply, errorString);
}

int SessionManager::sendGetRequest(const QUrl& url, const QString &objectName, QByteArray& reply, QByteArray& errorString)
{
    return sendGetRequest(url, objectName, QnRequestParamList(), reply, errorString);
}

int SessionManager::sendGetRequest(const QUrl& url, const QString &objectName, const QnRequestParamList &params, QByteArray& reply, QByteArray& errorString)
{
    SyncRequestProcessor syncProcessor(this);
    syncProcessor.moveToThread(this->thread());
    sendAsyncGetRequest(url, objectName, params, &syncProcessor, "finished");
    return syncProcessor.wait(reply, errorString);
}

void SessionManager::sendAsyncGetRequest(const QUrl& url, const QString &objectName, QObject *target, const char *slot)
{
    sendAsyncGetRequest(url, objectName, QnRequestParamList(), target, slot);
}

void SessionManager::doSendAsyncGetRequest(const QUrl& url, const QString &objectName, const QnRequestParamList &params, QObject *target, const char *slot)
{
    QNetworkRequest request;
    request.setUrl(createApiUrl(url, objectName, params));
    request.setRawHeader("Connection", "close");
    request.setRawHeader("Authorization", "Basic " + url.userInfo().toLatin1().toBase64());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "text/xml");

    QNetworkReply* reply = m_accessManager.get(request);
    reply->setProperty("target", QVariant::fromValue<QObject*>(target));
    reply->setProperty("slot", QVariant(QByteArray(slot)));
}

void SessionManager::sendAsyncGetRequest(const QUrl& url, const QString &objectName, const QnRequestParamList &params, QObject *target, const char *slot)
{
    emit asyncGetRequest(url, objectName, params, target, slot);
}

void SessionManager::sendAsyncPostRequest(const QUrl& url, const QString &objectName, const QByteArray& data, QObject *target, const char *slot)
{
    sendAsyncPostRequest(url, objectName, QnRequestParamList(), data, target, slot);
}

void SessionManager::doSendAsyncPostRequest(const QUrl& url, const QString &objectName, const QnRequestParamList &params, const QByteArray& data, QObject *target, const char *slot)
{
    QNetworkRequest request;
    request.setUrl(createApiUrl(url, objectName, params));
    request.setRawHeader("Connection", "close");
    request.setRawHeader("Authorization", "Basic " + url.userInfo().toLatin1().toBase64());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "text/xml");

    QNetworkReply* reply = m_accessManager.post(request, data);
    reply->setProperty("target", QVariant(QMetaType::QObjectStar, target));
    reply->setProperty("slot", QVariant(QByteArray(slot)));
}

void SessionManager::sendAsyncPostRequest(const QUrl& url, const QString &objectName, const QnRequestParamList &params, const QByteArray& data, QObject *target, const char *slot)
{
    emit asyncGetRequest(url, objectName, params, target, slot);
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
