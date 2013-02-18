#include "session_manager.h"

#include <cassert>

#include <QtCore/QUrl>
#include <QtCore/QBuffer>
#include <QtCore/QThread>
#include <QtNetwork/QNetworkReply>

#include "utils/common/warnings.h"
#include "utils/common/delete_later.h"
#include "common/common_meta_types.h"
#include "app_server_connection.h"

// -------------------------------------------------------------------------- //
// Reply processors
// -------------------------------------------------------------------------- //
void SessionManagerReplyProcessor::at_replyReceived()
{
    QNetworkReply *reply = (QNetworkReply *)sender();

    emit finished(QnHTTPRawResponse(reply->error(), reply->rawHeaderPairs(), reply->readAll(), reply->errorString().toAscii()), m_handle);

    qnDeleteLater(reply);
    qnDeleteLater(this);
}

void SyncRequestProcessor::at_finished(const QnHTTPRawResponse& response, int handle)
{
    Q_UNUSED(handle)

    m_response = response;

    QMutexLocker locker(&m_mutex);
    m_finished = true;
    m_condition.wakeOne();
}

int SyncRequestProcessor::wait(QnHTTPRawResponse& response)
{
    QMutexLocker locker(&m_mutex);
    while (!m_finished) {
        m_condition.wait(&m_mutex);
    }

    response = m_response;
    return m_response.status;
}

void SyncRequestProcessor::at_destroy()
{
    QMutexLocker locker(&m_mutex);
    m_finished = true;
    m_condition.wakeOne();
}


// -------------------------------------------------------------------------- //
// QnSessionManager
// -------------------------------------------------------------------------- //
QAtomicInt QnSessionManager::s_handle(1);
Q_GLOBAL_STATIC(QnSessionManager, qn_sessionManager_instance);


QnSessionManager::QnSessionManager(QObject *parent): 
    QObject(parent),
    m_accessManager(0)
{
    connect(this, SIGNAL(asyncGetRequest(SessionManagerReplyProcessor*, QUrl, QString, QnRequestHeaderList, QnRequestParamList)), this, SLOT(doSendAsyncGetRequest(SessionManagerReplyProcessor*, QUrl, QString, QnRequestHeaderList, QnRequestParamList)));
    connect(this, SIGNAL(asyncPostRequest(SessionManagerReplyProcessor*, QUrl, QString, QnRequestHeaderList, QnRequestParamList, QByteArray)), this, SLOT(doSendAsyncPostRequest(SessionManagerReplyProcessor*, QUrl, QString, QnRequestHeaderList, QnRequestParamList, QByteArray)));
    connect(this, SIGNAL(asyncDeleteRequest(SessionManagerReplyProcessor*, QUrl, QString, int)), this, SLOT(doSendAsyncDeleteRequest(SessionManagerReplyProcessor*, QUrl, QString, int)));
    connect(this, SIGNAL(aboutToBeStopped()), this, SLOT(doStop()));
    connect(this, SIGNAL(aboutToBeStarted()), this, SLOT(doStart()));
}

QnSessionManager::~QnSessionManager()
{
    doStop();
}

QnSessionManager *QnSessionManager::instance()
{
    return qn_sessionManager_instance();
}

void QnSessionManager::start()
{
    emit aboutToBeStarted();
}

void QnSessionManager::stop()
{
    emit aboutToBeStopped();
}

bool QnSessionManager::isReady() const
{
    QMutexLocker locker(&m_accessManagerMutex);
    return m_accessManager != 0;
}

int QnSessionManager::sendGetRequest(const QUrl& url, const QString &objectName, QnHTTPRawResponse& response)
{
    return sendGetRequest(url, objectName, QnRequestHeaderList(), QnRequestParamList(), response);
}

int QnSessionManager::sendGetRequest(const QUrl& url, const QString &objectName, const QnRequestHeaderList &headers, const QnRequestParamList &params, QnHTTPRawResponse& response)
{
    SyncRequestProcessor syncProcessor;
    syncProcessor.moveToThread(this->thread());

    {
        QMutexLocker locker(&m_accessManagerMutex);

        if (!m_accessManager)
        {
            response.errorString = "Network client is not ready yet";
            return -1;
        }

        connect(m_accessManager, SIGNAL(destroyed()), &syncProcessor, SLOT(at_destroy()));
    }

    sendAsyncGetRequest(url, objectName, headers, params, &syncProcessor, SLOT(at_finished(QnHTTPRawResponse,int)));
    return syncProcessor.wait(response);
}

int QnSessionManager::sendPostRequest(const QUrl& url, const QString &objectName, const QByteArray& data, QnHTTPRawResponse& response)
{
    return sendPostRequest(url, objectName, QnRequestHeaderList(), QnRequestParamList(), data, response);
}

int QnSessionManager::sendPostRequest(const QUrl& url, const QString &objectName, const QnRequestHeaderList &headers, const QnRequestParamList &params, const QByteArray& data, QnHTTPRawResponse& response)
{
    SyncRequestProcessor syncProcessor;
    syncProcessor.moveToThread(this->thread());

    {
        QMutexLocker locker(&m_accessManagerMutex);

        if (!m_accessManager)
        {
            response.errorString = "Network client is not ready yet.";
            return -1;
        }

        connect(m_accessManager, SIGNAL(destroyed()), &syncProcessor, SLOT(at_destroy()));
    }

    sendAsyncPostRequest(url, objectName, headers, params, data, &syncProcessor, SLOT(at_finished(QnHTTPRawResponse,int)));
    return syncProcessor.wait(response);
}

int QnSessionManager::sendAsyncGetRequest(const QUrl& url, const QString &objectName, QObject *target, const char *slot, Qt::ConnectionType connectionType)
{
    return sendAsyncGetRequest(url, objectName, QnRequestHeaderList(), QnRequestParamList(), target, slot, connectionType);
}

int QnSessionManager::sendAsyncGetRequest(const QUrl& url, const QString &objectName, const QnRequestHeaderList &headers, const QnRequestParamList &params, QObject *target, const char *slot, Qt::ConnectionType connectionType)
{
    int handle = s_handle.fetchAndAddAcquire(1);

    // We need to create reply processor here as target could not exist when doAsyncGetRequest gets called
    SessionManagerReplyProcessor* replyProcessor = new SessionManagerReplyProcessor(handle);
    replyProcessor->moveToThread(this->thread());
    connect(replyProcessor, SIGNAL(finished(QnHTTPRawResponse, int)), target, slot, connectionType == Qt::AutoConnection ? Qt::QueuedConnection : connectionType);
    connect(replyProcessor, SIGNAL(finished(QnHTTPRawResponse, int)), this, SLOT(processReply(QnHTTPRawResponse, int)), Qt::QueuedConnection);

    emit asyncGetRequest(replyProcessor, url, objectName, headers, params);

    return handle;
}

int QnSessionManager::sendAsyncPostRequest(const QUrl& url, const QString &objectName, const QByteArray& data, QObject *target, const char *slot, Qt::ConnectionType connectionType)
{
    return sendAsyncPostRequest(url, objectName, QnRequestHeaderList(), QnRequestParamList(), data, target, slot, connectionType);
}

int QnSessionManager::sendAsyncPostRequest(const QUrl& url, const QString &objectName, const QnRequestHeaderList &headers, const QnRequestParamList &params, const QByteArray& data, QObject *target, const char *slot, Qt::ConnectionType connectionType)
{
    int handle = s_handle.fetchAndAddAcquire(1);

    // We need to create reply processor here as target could not exist when doAsyncGetRequest gets called
    SessionManagerReplyProcessor* replyProcessor = new SessionManagerReplyProcessor(handle);
    replyProcessor->moveToThread(this->thread());
    connect(replyProcessor, SIGNAL(finished(QnHTTPRawResponse, int)), target, slot, connectionType == Qt::AutoConnection ? Qt::QueuedConnection : connectionType);
    connect(replyProcessor, SIGNAL(finished(QnHTTPRawResponse, int)), this, SLOT(processReply(QnHTTPRawResponse, int)), Qt::QueuedConnection);

    emit asyncPostRequest(replyProcessor, url, objectName, headers, params, data);

    return handle;
}

int QnSessionManager::sendAsyncDeleteRequest(const QUrl& url, const QString &objectName, int id, QObject *target, const char *slot, Qt::ConnectionType connectionType)
{
    int handle = s_handle.fetchAndAddAcquire(1);

    // We need to create reply processor here as target could not exist when doAsyncDeleteRequest gets called
    SessionManagerReplyProcessor* replyProcessor = new SessionManagerReplyProcessor(handle);
    replyProcessor->moveToThread(this->thread());
    connect(replyProcessor, SIGNAL(finished(QnHTTPRawResponse, int)), target, slot, connectionType == Qt::AutoConnection ? Qt::QueuedConnection : connectionType);
    connect(replyProcessor, SIGNAL(finished(QnHTTPRawResponse, int)), this, SLOT(processReply(QnHTTPRawResponse, int)), Qt::QueuedConnection);

    emit asyncDeleteRequest(replyProcessor, url, objectName, id);

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

void QnSessionManager::processReply(const QnHTTPRawResponse& response, int) {
    emit replyReceived(response.status);
}


// -------------------------------------------------------------------------- //
// Grunt work handlers
// -------------------------------------------------------------------------- //
void QnSessionManager::doStart()
{
    assert(QThread::currentThread() == this->thread());

    QMutexLocker locker(&m_accessManagerMutex);

    if (m_accessManager)
        return;

    m_accessManager = new QNetworkAccessManager(this);
}

void QnSessionManager::doStop()
{
    //assert(QThread::currentThread() == this->thread()); // TODO: #Elric this one is called from dtor, resolve.

    QMutexLocker locker(&m_accessManagerMutex);

    if (!m_accessManager)
        return;

    m_accessManager->setParent(0);
    m_accessManager->deleteLater();
    m_accessManager = 0;
}

// TODO: #Elric merge into a single method.
void QnSessionManager::doSendAsyncGetRequest(SessionManagerReplyProcessor* replyProcessor, const QUrl& url, const QString &objectName, const QnRequestHeaderList &headers, const QnRequestParamList &params)
{
    assert(QThread::currentThread() == this->thread());

    if (!m_accessManager)
    {
        qWarning() << "doSendAsyncGetRequest is called, while accessManager = 0";
        return;
    }

    QNetworkRequest request;
    request.setUrl(createApiUrl(url, objectName, params));

    foreach (QnRequestHeader header, headers) {
        request.setRawHeader(header.first.toAscii(), header.second.toUtf8());
    }

    request.setRawHeader("Authorization", "Basic " + url.userInfo().toLatin1().toBase64());
    request.setHeader(QNetworkRequest::ContentTypeHeader, QLatin1String("text/xml"));

    QNetworkReply* reply = m_accessManager->get(request);
    connect(reply, SIGNAL(finished()), replyProcessor, SLOT(at_replyReceived()));
    connect(reply, SIGNAL(sslErrors(QList<QSslError>)), reply, SLOT(ignoreSslErrors()));
}

void QnSessionManager::doSendAsyncPostRequest(SessionManagerReplyProcessor* replyProcessor, const QUrl& url, const QString &objectName, const QnRequestHeaderList &headers, const QnRequestParamList &params, const QByteArray& data)
{
    assert(QThread::currentThread() == this->thread());

    if (!m_accessManager)
    {
        qWarning() << "doSendAsyncPostRequest is called, while accessManager = 0";
        return;
    }

    QNetworkRequest request;
    request.setUrl(createApiUrl(url, objectName, params));

    foreach (QnRequestHeader header, headers) {
        request.setRawHeader(header.first.toAscii(), header.second.toUtf8());
    }

    request.setRawHeader("Authorization", "Basic " + url.userInfo().toLatin1().toBase64());
    request.setHeader(QNetworkRequest::ContentTypeHeader, QLatin1String("text/xml"));

    QNetworkReply* reply = m_accessManager->post(request, data);
    connect(reply, SIGNAL(finished()), replyProcessor, SLOT(at_replyReceived()));
    connect(reply, SIGNAL(sslErrors(QList<QSslError>)), reply, SLOT(ignoreSslErrors()));
}

void QnSessionManager::doSendAsyncDeleteRequest(SessionManagerReplyProcessor* replyProcessor, const QUrl& url, const QString &objectName, int id)
{
    assert(QThread::currentThread() == this->thread());

    if (!m_accessManager)
    {
        qWarning() << "doSendAsyncDeleteRequest is called, while accessManager = 0";
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