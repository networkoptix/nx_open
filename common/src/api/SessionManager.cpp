#include "SessionManager.h"

#include <QtCore/QBuffer>
#include <QtCore/QThread>

#include <QtNetwork/QNetworkReply>

#include "utils/common/warnings.h"

SessionManager* SessionManager::m_instance = 0;
QAtomicInt SessionManager::m_handle(1);

void SessionManagerReplyProcessor::targetDestroyed(QObject* target)
{
    m_target = 0;
}

void SessionManagerReplyProcessor::at_replyReceived()
{
    QNetworkReply *reply = (QNetworkReply *)sender();

    if (m_target)
        connect(this, SIGNAL(finished(int,QByteArray,QByteArray, int)), m_target, m_slot, Qt::QueuedConnection);

    emit finished(reply->error(), reply->readAll(), reply->errorString().toAscii(), m_handle);

    reply->deleteLater();
    deleteLater();
}

void SyncRequestProcessor::finished(int status, const QByteArray& reply, const QByteArray& errorString, int handle)
{
    Q_UNUSED(handle)

    m_status = status;
    m_reply = reply;
    m_errorString = errorString;

    QMutexLocker locker(&m_mutex);
    m_finished = true;
    m_condition.wakeOne();
}

int SyncRequestProcessor::wait(QByteArray& reply, QByteArray& errorString)
{
    QMutexLocker locker(&m_mutex);
    while (!m_finished) {
        m_condition.wait(&m_mutex);
    }

    reply = m_reply;
    errorString = m_errorString;
    return m_status;
}

void SyncRequestProcessor::at_destroy()
{
    QMutexLocker locker(&m_mutex);
    m_finished = true;
    m_condition.wakeOne();
}

SessionManager::SessionManager()
    : m_accessManager(0)
{
    qRegisterMetaType<QnRequestParamList>("QnRequestParamList");

    connect(this, SIGNAL(asyncGetRequest(SessionManagerReplyProcessor*, QUrl, QString,QnRequestParamList,QObject*,const char*, int)), this, SLOT(doSendAsyncGetRequest(SessionManagerReplyProcessor*, QUrl, QString,QnRequestParamList,QObject*,const char*, int)));
    connect(this, SIGNAL(asyncPostRequest(SessionManagerReplyProcessor*, QUrl, QString,QnRequestParamList,QByteArray,QObject*,const char*, int)), this, SLOT(doSendAsyncPostRequest(SessionManagerReplyProcessor*, QUrl, QString,QnRequestParamList,QByteArray,QObject*,const char*, int)));
    connect(this, SIGNAL(asyncDeleteRequest(SessionManagerReplyProcessor*, QUrl, QString,int,QObject*,const char*, int)), this, SLOT(doSendAsyncDeleteRequest(SessionManagerReplyProcessor*, QUrl, QString,int,QObject*,const char*, int)));

    connect(this, SIGNAL(stopSignal()), this, SLOT(doStop()));
    connect(this, SIGNAL(startSignal()), this, SLOT(doStart()));
}

SessionManager::~SessionManager()
{
    if(QThread::currentThread() != this->thread())
        qnWarning("Deleting session manager from another thread is dangerous and may lead to unexpected crashes.");
}

void SessionManager::start()
{
    emit startSignal();
}

void SessionManager::stop()
{
    emit stopSignal();
}

void SessionManager::doStart()
{
    QMutexLocker locker(&m_accessManagerMutex);

    if (m_accessManager)
        return;

    m_accessManager = new QNetworkAccessManager(this);
}

void SessionManager::doStop()
{
    QMutexLocker locker(&m_accessManagerMutex);

    if (m_accessManager)
    {
        m_accessManager->deleteLater();
        m_accessManager = 0;
    }
}

SessionManager *SessionManager::instance()
{
    if (m_instance)
        return m_instance;

    m_instance = new SessionManager();
    return m_instance;
}

int SessionManager::testConnectionAsync(const QUrl& url, QObject* receiver, const char *slot)
{
    return sendAsyncGetRequest(url, "ping", receiver, slot);
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

int SessionManager::sendPostRequest(const QUrl& url, const QString &objectName, const QnRequestParamList &params, const QByteArray& data, QByteArray &reply, QByteArray& errorString)
{
    SyncRequestProcessor syncProcessor;
    syncProcessor.moveToThread(this->thread());

    {
        QMutexLocker locker(&m_accessManagerMutex);

        if (!m_accessManager)
        {
            errorString = "Network client is not ready yet.";
            return -1;
        }

        connect(m_accessManager, SIGNAL(destroyed()), &syncProcessor, SLOT(at_destroy()));
    }

    sendAsyncPostRequest(url, objectName, params, data, &syncProcessor, SLOT(finished(int,QByteArray,QByteArray,int)));
    return syncProcessor.wait(reply, errorString);
}

int SessionManager::sendGetRequest(const QUrl& url, const QString &objectName, QByteArray& reply, QByteArray& errorString)
{
    return sendGetRequest(url, objectName, QnRequestParamList(), reply, errorString);
}

int SessionManager::sendGetRequest(const QUrl& url, const QString &objectName, const QnRequestParamList &params, QByteArray& reply, QByteArray& errorString)
{
    SyncRequestProcessor syncProcessor;
    syncProcessor.moveToThread(this->thread());

    {
        QMutexLocker locker(&m_accessManagerMutex);

        if (!m_accessManager)
        {
            errorString = "Network client is not ready yet";
            return -1;
        }

        connect(m_accessManager, SIGNAL(destroyed()), &syncProcessor, SLOT(at_destroy()));
    }

    sendAsyncGetRequest(url, objectName, params, &syncProcessor, SLOT(finished(int,QByteArray,QByteArray,int)));
    return syncProcessor.wait(reply, errorString);
}

int SessionManager::sendAsyncGetRequest(const QUrl& url, const QString &objectName, QObject *target, const char *slot)
{
    return sendAsyncGetRequest(url, objectName, QnRequestParamList(), target, slot);
}

void SessionManager::doSendAsyncGetRequest(SessionManagerReplyProcessor* replyProcessor, const QUrl& url, const QString &objectName, const QnRequestParamList &params, QObject *target, const char *slot, int handle)
{
    if (!m_accessManager)
    {
        qWarning() << "doSendAsyncPostRequest is called, while accessManager = 0";
        return;
    }

    QNetworkRequest request;
    request.setUrl(createApiUrl(url, objectName, params));
    request.setRawHeader("Authorization", "Basic " + url.userInfo().toLatin1().toBase64());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "text/xml");

    QNetworkReply* reply = m_accessManager->get(request);
    connect(reply, SIGNAL(finished()), replyProcessor, SLOT(at_replyReceived()));
}

void SessionManager::doSendAsyncDeleteRequest(SessionManagerReplyProcessor* replyProcessor, const QUrl& url, const QString &objectName, int id, QObject *target, const char *slot, int handle)
{
    if (!m_accessManager)
    {
        qWarning() << "doSendAsyncPostRequest is called, while accessManager = 0";
        return;
    }

    QNetworkRequest request;
    request.setUrl(createApiUrl(url, objectName, QnRequestParamList() << QnRequestParam("id", QString::number(id))));
    request.setRawHeader("Authorization", "Basic " + url.userInfo().toLatin1().toBase64());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "text/xml");

    QNetworkReply* reply = m_accessManager->deleteResource(request);
    connect(reply, SIGNAL(finished()), replyProcessor, SLOT(at_replyReceived()));
}

int SessionManager::sendAsyncGetRequest(const QUrl& url, const QString &objectName, const QnRequestParamList &params, QObject *target, const char *slot)
{
    int handle = m_handle.fetchAndAddAcquire(1);

    // We need to create reply processor here as target could not exist when doAsyncGetRequest gets called
    SessionManagerReplyProcessor* replyProcessor = new SessionManagerReplyProcessor(0, target, slot, handle);
    replyProcessor->moveToThread(thread());
    connect(target, SIGNAL(destroyed(QObject*)), replyProcessor, SLOT(targetDestroyed(QObject*)));

    emit asyncGetRequest(replyProcessor, url, objectName, params, target, slot, handle);

    return handle;
}

int SessionManager::sendAsyncPostRequest(const QUrl& url, const QString &objectName, const QByteArray& data, QObject *target, const char *slot)
{
    return sendAsyncPostRequest(url, objectName, QnRequestParamList(), data, target, slot);
}

int SessionManager::sendAsyncDeleteRequest(const QUrl& url, const QString &objectName, int id, QObject *target, const char *slot)
{
    int handle = m_handle.fetchAndAddAcquire(1);

    // We need to create reply processor here as target could not exist when doAsyncDeleteRequest gets called
    SessionManagerReplyProcessor* replyProcessor = new SessionManagerReplyProcessor(0, target, slot, handle);
    replyProcessor->moveToThread(thread());
    connect(target, SIGNAL(destroyed(QObject*)), replyProcessor, SLOT(targetDestroyed(QObject*)));

    emit asyncDeleteRequest(replyProcessor, url, objectName, id, target, slot, handle);

    return handle;
}

void SessionManager::doSendAsyncPostRequest(SessionManagerReplyProcessor* replyProcessor, const QUrl& url, const QString &objectName, const QnRequestParamList &params, const QByteArray& data, QObject *target, const char *slot, int handle)
{
    if (!m_accessManager)
    {
        qWarning() << "doSendAsyncPostRequest is called, while accessManager = 0";
        return;
    }

    QNetworkRequest request;
    request.setUrl(createApiUrl(url, objectName, params));
    //request.setRawHeader("Connection", "close");
    request.setRawHeader("Authorization", "Basic " + url.userInfo().toLatin1().toBase64());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "text/xml");

    QNetworkReply* reply = m_accessManager->post(request, data);
    connect(reply, SIGNAL(finished()), replyProcessor, SLOT(at_replyReceived()));
}

int SessionManager::sendAsyncPostRequest(const QUrl& url, const QString &objectName, const QnRequestParamList &params, const QByteArray& data, QObject *target, const char *slot)
{
    int handle = m_handle.fetchAndAddAcquire(1);

    // We need to create reply processor here as target could not exist when doAsyncGetRequest gets called
    SessionManagerReplyProcessor* replyProcessor = new SessionManagerReplyProcessor(0, target, slot, handle);
    replyProcessor->moveToThread(thread());
    connect(target, SIGNAL(destroyed(QObject*)), replyProcessor, SLOT(targetDestroyed(QObject*)));

    emit asyncPostRequest(replyProcessor, url, objectName, params, data, target, slot, handle);

    return handle;
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
