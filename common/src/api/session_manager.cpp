#include "session_manager.h"

#include <QtCore/QBuffer>
#include <QtCore/QThread>

#include <QtNetwork/QNetworkReply>

#include "utils/common/warnings.h"
#include "common/common_meta_types.h"

Q_GLOBAL_STATIC(QnSessionManager, qn_sessionManagerInstance);
QAtomicInt QnSessionManager::m_handle(1);

void SessionManagerReplyProcessor::at_replyReceived()
{
    QNetworkReply *reply = (QNetworkReply *)sender();

    emit finished(reply->error(), reply->readAll(), reply->errorString().toAscii(), m_handle);

    reply->deleteLater();
    deleteLater();
}

void SyncRequestProcessor::at_finished(int status, const QByteArray &reply, const QByteArray &errorString, int handle)
{
    Q_UNUSED(handle)

    m_status = status;
    m_reply = reply;
    m_errorString = errorString;

    QMutexLocker locker(&m_mutex);
    m_finished = true;
    m_condition.wakeOne();
}

int SyncRequestProcessor::wait(QByteArray &reply, QByteArray &errorString)
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

QnSessionManager::QnSessionManager()
    : m_accessManager(0)
{
    QnCommonMetaTypes::initilize();

    connect(this, SIGNAL(asyncGetRequest(SessionManagerReplyProcessor*, QUrl, QString, QnRequestParamList, QObject*, const char*, int)), this, SLOT(doSendAsyncGetRequest(SessionManagerReplyProcessor*, QUrl, QString,QnRequestParamList,QObject*,const char*, int)));
    connect(this, SIGNAL(asyncPostRequest(SessionManagerReplyProcessor*, QUrl, QString, QnRequestParamList, QByteArray, QObject*, const char*, int)), this, SLOT(doSendAsyncPostRequest(SessionManagerReplyProcessor*, QUrl, QString,QnRequestParamList,QByteArray,QObject*,const char*, int)));
    connect(this, SIGNAL(asyncDeleteRequest(SessionManagerReplyProcessor*, QUrl, QString, int, QObject*, const char*, int)), this, SLOT(doSendAsyncDeleteRequest(SessionManagerReplyProcessor*, QUrl, QString,int,QObject*,const char*, int)));

    connect(this, SIGNAL(aboutToBeStopped()), this, SLOT(doStop()));
    connect(this, SIGNAL(aboutToBeStarted()), this, SLOT(doStart()));
}

QnSessionManager::~QnSessionManager()
{
    doStop();
    /*
    if(QThread::currentThread() != this->thread()) {
        qnWarning("Deleting session manager from another thread is dangerous and may lead to unexpected crashes.");
    }
    */
}

QnSessionManager *QnSessionManager::instance()
{
    return qn_sessionManagerInstance();
}

void QnSessionManager::start()
{
    emit aboutToBeStarted();
}

void QnSessionManager::stop()
{
    emit aboutToBeStopped();
}

void QnSessionManager::doStart()
{
    QMutexLocker locker(&m_accessManagerMutex);

    if (m_accessManager)
        return;

    m_accessManager = new QNetworkAccessManager(this);
}

void QnSessionManager::doStop()
{
    QMutexLocker locker(&m_accessManagerMutex);

    if (m_accessManager)
    {
        m_accessManager->setParent(0);
        m_accessManager->deleteLater();
        m_accessManager = 0;
    }
}

QUrl QnSessionManager::createApiUrl(const QUrl& baseUrl, const QString &objectName, const QnRequestParamList &params) const
{
    QUrl url(baseUrl);

    QString path = QLatin1String("api/") + objectName + QLatin1Char('/');
    url.setPath(path);

    for (int i = 0; i < params.count(); i++){
        QPair<QString, QString> param = params[i];
        url.addQueryItem(param.first, param.second);
    }
    return url;
}

int QnSessionManager::sendPostRequest(const QUrl& url, const QString &objectName, const QByteArray& data, QByteArray &reply, QByteArray& errorString)
{
    return sendPostRequest(url, objectName, QnRequestParamList(), data, reply, errorString);
}

int QnSessionManager::sendPostRequest(const QUrl& url, const QString &objectName, const QnRequestParamList &params, const QByteArray& data, QByteArray &reply, QByteArray& errorString)
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

    sendAsyncPostRequest(url, objectName, params, data, &syncProcessor, SLOT(at_finished(int,QByteArray,QByteArray,int)));
    return syncProcessor.wait(reply, errorString);
}

int QnSessionManager::sendGetRequest(const QUrl& url, const QString &objectName, QByteArray& reply, QByteArray& errorString)
{
    return sendGetRequest(url, objectName, QnRequestParamList(), reply, errorString);
}

bool QnSessionManager::isReady() const
{
    QMutexLocker locker(&m_accessManagerMutex);
    return m_accessManager != 0;
}

int QnSessionManager::sendGetRequest(const QUrl& url, const QString &objectName, const QnRequestParamList &params, QByteArray& reply, QByteArray& errorString)
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

    sendAsyncGetRequest(url, objectName, params, &syncProcessor, SLOT(at_finished(int,QByteArray,QByteArray,int)));
    return syncProcessor.wait(reply, errorString);
}

int QnSessionManager::sendAsyncGetRequest(const QUrl& url, const QString &objectName, QObject *target, const char *slot)
{
    return sendAsyncGetRequest(url, objectName, QnRequestParamList(), target, slot);
}

void QnSessionManager::doSendAsyncGetRequest(SessionManagerReplyProcessor* replyProcessor, const QUrl& url, const QString &objectName, const QnRequestParamList &params, QObject *target, const char *slot, int handle)
{
    Q_UNUSED(target);
    Q_UNUSED(slot);
    Q_UNUSED(handle);

    if (!m_accessManager)
    {
        qWarning() << "doSendAsyncPostRequest is called, while accessManager = 0";
        return;
    }

    QNetworkRequest request;
    request.setUrl(createApiUrl(url, objectName, params));
    request.setRawHeader("Authorization", "Basic " + url.userInfo().toLatin1().toBase64());
    request.setHeader(QNetworkRequest::ContentTypeHeader, QLatin1String("text/xml"));

    QNetworkReply* reply = m_accessManager->get(request);
    connect(reply, SIGNAL(finished()), replyProcessor, SLOT(at_replyReceived()));
    connect(reply, SIGNAL(sslErrors(QList<QSslError>)), reply, SLOT(ignoreSslErrors()));
}

void QnSessionManager::doSendAsyncDeleteRequest(SessionManagerReplyProcessor* replyProcessor, const QUrl& url, const QString &objectName, int id, QObject *target, const char *slot, int handle)
{
    Q_UNUSED(target);
    Q_UNUSED(slot);
    Q_UNUSED(handle);

    if (!m_accessManager)
    {
        qWarning() << "doSendAsyncPostRequest is called, while accessManager = 0";
        return;
    }

    QNetworkRequest request;
    request.setUrl(createApiUrl(url, objectName, QnRequestParamList() << QnRequestParam("id", QString::number(id))));
    request.setRawHeader("Authorization", "Basic " + url.userInfo().toLatin1().toBase64());
    request.setHeader(QNetworkRequest::ContentTypeHeader, QLatin1String("text/xml"));

    QNetworkReply* reply = m_accessManager->deleteResource(request);
    connect(reply, SIGNAL(finished()), replyProcessor, SLOT(at_replyReceived()));
    connect(reply, SIGNAL(sslErrors(QList<QSslError>)), reply, SLOT(ignoreSslErrors()));
}

int QnSessionManager::sendAsyncGetRequest(const QUrl& url, const QString &objectName, const QnRequestParamList &params, QObject *target, const char *slot)
{
    int handle = m_handle.fetchAndAddAcquire(1);

    // We need to create reply processor here as target could not exist when doAsyncGetRequest gets called
    SessionManagerReplyProcessor* replyProcessor = new SessionManagerReplyProcessor(0, target, slot, handle);
    replyProcessor->moveToThread(thread());
    connect(replyProcessor, SIGNAL(finished(int, QByteArray, QByteArray, int)), target, slot, Qt::QueuedConnection);
    connect(replyProcessor, SIGNAL(finished(int, QByteArray, QByteArray, int)), this, SLOT(processReply(int, const QByteArray &, const QByteArray &, int)), Qt::QueuedConnection);

    emit asyncGetRequest(replyProcessor, url, objectName, params, target, slot, handle);

    return handle;
}

int QnSessionManager::sendAsyncPostRequest(const QUrl& url, const QString &objectName, const QByteArray& data, QObject *target, const char *slot)
{
    return sendAsyncPostRequest(url, objectName, QnRequestParamList(), data, target, slot);
}

int QnSessionManager::sendAsyncDeleteRequest(const QUrl& url, const QString &objectName, int id, QObject *target, const char *slot)
{
    int handle = m_handle.fetchAndAddAcquire(1);

    // We need to create reply processor here as target could not exist when doAsyncDeleteRequest gets called
    SessionManagerReplyProcessor* replyProcessor = new SessionManagerReplyProcessor(0, target, slot, handle);
    replyProcessor->moveToThread(thread());
    connect(replyProcessor, SIGNAL(finished(int, QByteArray, QByteArray, int)), target, slot, Qt::QueuedConnection);
    connect(replyProcessor, SIGNAL(finished(int, QByteArray, QByteArray, int)), this, SLOT(processReply(int, const QByteArray &, const QByteArray &, int)), Qt::QueuedConnection);

    emit asyncDeleteRequest(replyProcessor, url, objectName, id, target, slot, handle);

    return handle;
}

void QnSessionManager::doSendAsyncPostRequest(SessionManagerReplyProcessor* replyProcessor, const QUrl& url, const QString &objectName, const QnRequestParamList &params, const QByteArray& data, QObject *target, const char *slot, int handle)
{
    Q_UNUSED(target);
    Q_UNUSED(slot);
    Q_UNUSED(handle);

    if (!m_accessManager)
    {
        qWarning() << "doSendAsyncPostRequest is called, while accessManager = 0";
        return;
    }

    QNetworkRequest request;
    request.setUrl(createApiUrl(url, objectName, params));
    request.setRawHeader("Authorization", "Basic " + url.userInfo().toLatin1().toBase64());
    request.setHeader(QNetworkRequest::ContentTypeHeader, QLatin1String("text/xml"));

    QNetworkReply* reply = m_accessManager->post(request, data);
    connect(reply, SIGNAL(finished()), replyProcessor, SLOT(at_replyReceived()));
    connect(reply, SIGNAL(sslErrors(QList<QSslError>)), reply, SLOT(ignoreSslErrors()));
}

int QnSessionManager::sendAsyncPostRequest(const QUrl& url, const QString &objectName, const QnRequestParamList &params, const QByteArray& data, QObject *target, const char *slot)
{
    int handle = m_handle.fetchAndAddAcquire(1);

    // We need to create reply processor here as target could not exist when doAsyncGetRequest gets called
    SessionManagerReplyProcessor* replyProcessor = new SessionManagerReplyProcessor(0, target, slot, handle);
    replyProcessor->moveToThread(thread());
    connect(replyProcessor, SIGNAL(finished(int, QByteArray, QByteArray, int)), target, slot, Qt::QueuedConnection);
    connect(replyProcessor, SIGNAL(finished(int, QByteArray, QByteArray, int)), this, SLOT(processReply(int, const QByteArray &, const QByteArray &, int)), Qt::QueuedConnection);

    emit asyncPostRequest(replyProcessor, url, objectName, params, data, target, slot, handle);

    return handle;
}

QByteArray QnSessionManager::formatNetworkError(int error)
{
    QByteArray errorValue;

    QMetaObject metaObject = QNetworkReply::staticMetaObject;
    const int idx = metaObject.indexOfEnumerator("NetworkError");
    if (idx != -1)
        errorValue = metaObject.enumerator(idx).valueToKey(error);

    return errorValue;
}

void QnSessionManager::processReply(int status, const QByteArray &, const QByteArray &, int) {
    emit replyReceived(status);
}

