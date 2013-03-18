#include "session_manager.h"

#include <cassert>

#include <QtCore/QUrl>
#include <QtCore/QBuffer>
#include <QtCore/QThread>
#include <QtCore/QScopedPointer>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkAccessManager>

#include "utils/common/warnings.h"
#include "utils/common/delete_later.h"
#include "utils/common/object_thread_puller.h"
#include "common/common_module.h"


// -------------------------------------------------------------------------- //
// Reply processors
// -------------------------------------------------------------------------- //
void QnSessionManagerAsyncReplyProcessor::at_replyReceived() {
    QNetworkReply *reply = (QNetworkReply *)sender();

    QString errorString = reply->errorString();
    // Common EC error looks like:
    // "Error downloading https://user:password@host:port/path - server replied: INTERNAL SERVER ERROR"
    // displaying plaint-text password is unsecure and strongly not recommended
    if (errorString.indexOf(QLatin1String("@")) > 0 && errorString.indexOf(QLatin1String(":")) > 0) {
        int n = errorString.lastIndexOf(QLatin1String(":"));
        errorString = errorString.mid(n + 1).trimmed();
    }
    emit finished(QnHTTPRawResponse(reply->error(), reply->rawHeaderPairs(), reply->readAll(), errorString.toAscii()), m_handle);

    qnDeleteLater(reply);
    qnDeleteLater(this);
}

void QnSessionManagerSyncReplyProcessor::at_finished(const QnHTTPRawResponse& response, int handle) {
    Q_UNUSED(handle)

    m_response = response;

    QMutexLocker locker(&m_mutex);
    m_finished = true;
    m_condition.wakeOne();
}

int QnSessionManagerSyncReplyProcessor::wait(QnHTTPRawResponse& response) {
    QMutexLocker locker(&m_mutex);
    while (!m_finished) {
        m_condition.wait(&m_mutex);
    }

    response = m_response;
    return m_response.status;
}

void QnSessionManagerSyncReplyProcessor::at_destroy() {
    QMutexLocker locker(&m_mutex);
    m_finished = true;
    m_condition.wakeOne();
}


// -------------------------------------------------------------------------- //
// QnSessionManager
// -------------------------------------------------------------------------- //
QAtomicInt QnSessionManager::s_handle(1);

QnSessionManager::QnSessionManager(QObject *parent): 
    QObject(parent),
    m_accessManager(NULL),
    m_thread(new QThread())
{
    connect(this, SIGNAL(asyncRequestQueued(int, QnSessionManagerAsyncReplyProcessor*, QUrl, QString, QnRequestHeaderList, QnRequestParamList, QByteArray)), this, SLOT(at_asyncRequestQueued(int, QnSessionManagerAsyncReplyProcessor*, QUrl, QString, QnRequestHeaderList, QnRequestParamList, QByteArray)));
    connect(this, SIGNAL(aboutToBeStopped()), this, SLOT(at_aboutToBeStopped()));
    connect(this, SIGNAL(aboutToBeStarted()), this, SLOT(at_aboutToBeStarted()));

    m_thread->setObjectName(lit("QnSessionManagerThread")); /* Name will be shown in debugger. */
    this->moveToThread(m_thread.data());
    m_thread->start();
}

QnSessionManager::~QnSessionManager() {
    disconnect(this, NULL, this, NULL);

    QScopedPointer<QnObjectThreadPuller> puller(new QnObjectThreadPuller());
    puller->pull(this);
    puller->wait(this);

    m_thread->exit();
    m_thread->wait();

    at_aboutToBeStopped();
}

QnSessionManager *QnSessionManager::instance() {
    return qnCommon->sessionManager();
}

void QnSessionManager::start() {
    emit aboutToBeStarted();
}

void QnSessionManager::stop() {
    emit aboutToBeStopped();
}

bool QnSessionManager::isReady() const {
    QMutexLocker locker(&m_accessManagerMutex);
    return m_accessManager != 0;
}

QByteArray QnSessionManager::formatNetworkError(int error) {
    QByteArray errorValue;

    QMetaObject metaObject = QNetworkReply::staticMetaObject;
    const int idx = metaObject.indexOfEnumerator("NetworkError");
    if (idx != -1)
        errorValue = metaObject.enumerator(idx).valueToKey(error);

    return errorValue;
}

QUrl QnSessionManager::createApiUrl(const QUrl& baseUrl, const QString &objectName, const QnRequestParamList &params) const {
    QUrl url(baseUrl);

    QString path = QLatin1String("api/") + objectName + QLatin1Char('/');
    url.setPath(path);

    for (int i = 0; i < params.count(); i++){
        QPair<QString, QString> param = params[i];
        url.addQueryItem(param.first, param.second);
    }
    return url;
}


// -------------------------------------------------------------------------- //
// QnSessionManager :: sync API
// -------------------------------------------------------------------------- //
int QnSessionManager::sendSyncRequest(int operation, const QUrl& url, const QString &objectName, const QnRequestHeaderList &headers, const QnRequestParamList &params, const QByteArray& data, QnHTTPRawResponse& response) {
    QScopedPointer<QnSessionManagerSyncReplyProcessor, QScopedPointerLaterDeleter> syncProcessor(new QnSessionManagerSyncReplyProcessor());
    syncProcessor->moveToThread(this->thread());

    {
        QMutexLocker locker(&m_accessManagerMutex);

        if (!m_accessManager) {
            response.errorString = "Network client is not ready yet.";
            return -1;
        }

        connect(m_accessManager, SIGNAL(destroyed()), syncProcessor.data(), SLOT(at_destroy()));
    }

    sendAsyncRequest(operation, url, objectName, headers, params, data, syncProcessor.data(), SLOT(at_finished(QnHTTPRawResponse,int)), Qt::AutoConnection);
    return syncProcessor->wait(response);
}

int QnSessionManager::sendGetRequest(const QUrl& url, const QString &objectName, QnHTTPRawResponse& response) {
    return sendGetRequest(url, objectName, QnRequestHeaderList(), QnRequestParamList(), response);
}

int QnSessionManager::sendGetRequest(const QUrl& url, const QString &objectName, const QnRequestHeaderList &headers, const QnRequestParamList &params, QnHTTPRawResponse& response) {
    return sendSyncRequest(QNetworkAccessManager::GetOperation, url, objectName, headers, params, QByteArray(), response);
}

int QnSessionManager::sendPostRequest(const QUrl& url, const QString &objectName, const QByteArray& data, QnHTTPRawResponse& response) {
    return sendPostRequest(url, objectName, QnRequestHeaderList(), QnRequestParamList(), data, response);
}

int QnSessionManager::sendPostRequest(const QUrl& url, const QString &objectName, const QnRequestHeaderList &headers, const QnRequestParamList &params, const QByteArray& data, QnHTTPRawResponse& response) {
    return sendSyncRequest(QNetworkAccessManager::PostOperation, url, objectName, headers, params, data, response);
}


// -------------------------------------------------------------------------- //
// QnSessionManager :: async API
// -------------------------------------------------------------------------- //
int QnSessionManager::sendAsyncRequest(int operation, const QUrl& url, const QString &objectName, const QnRequestHeaderList &headers, const QnRequestParamList &params, const QByteArray& data, QObject *target, const char *slot, Qt::ConnectionType connectionType) {
    int handle = s_handle.fetchAndAddAcquire(1);

    // We need to create reply processor here as target could not exist when doAsyncGetRequest gets called
    QnSessionManagerAsyncReplyProcessor* replyProcessor = new QnSessionManagerAsyncReplyProcessor(handle);
    replyProcessor->moveToThread(this->thread());
    connect(replyProcessor, SIGNAL(finished(QnHTTPRawResponse, int)), target, slot, connectionType == Qt::AutoConnection ? Qt::QueuedConnection : connectionType);
    connect(replyProcessor, SIGNAL(finished(QnHTTPRawResponse, int)), this, SLOT(at_replyProcessor_finished(QnHTTPRawResponse, int)), Qt::QueuedConnection);

    emit asyncRequestQueued(operation, replyProcessor, url, objectName, headers, params, data);

    return handle;
}

int QnSessionManager::sendAsyncGetRequest(const QUrl& url, const QString &objectName, QObject *target, const char *slot, Qt::ConnectionType connectionType) {
    return sendAsyncGetRequest(url, objectName, QnRequestHeaderList(), QnRequestParamList(), target, slot, connectionType);
}

int QnSessionManager::sendAsyncGetRequest(const QUrl& url, const QString &objectName, const QnRequestHeaderList &headers, const QnRequestParamList &params, QObject *target, const char *slot, Qt::ConnectionType connectionType) {
    return sendAsyncRequest(QNetworkAccessManager::GetOperation, url, objectName, headers, params, QByteArray(), target, slot, connectionType);
}

int QnSessionManager::sendAsyncPostRequest(const QUrl& url, const QString &objectName, const QByteArray& data, QObject *target, const char *slot, Qt::ConnectionType connectionType) {
    return sendAsyncPostRequest(url, objectName, QnRequestHeaderList(), QnRequestParamList(), data, target, slot, connectionType);
}

int QnSessionManager::sendAsyncPostRequest(const QUrl& url, const QString &objectName, const QnRequestHeaderList &headers, const QnRequestParamList &params, const QByteArray& data, QObject *target, const char *slot, Qt::ConnectionType connectionType) {
    return sendAsyncRequest(QNetworkAccessManager::PostOperation, url, objectName, headers, params, data, target, slot, connectionType);
}

int QnSessionManager::sendAsyncDeleteRequest(const QUrl& url, const QString &objectName, int id, QObject *target, const char *slot, Qt::ConnectionType connectionType) {
    QnRequestParamList params;
    params.push_back(QnRequestParam("id", QString::number(id)));
    return sendAsyncRequest(QNetworkAccessManager::DeleteOperation, url, objectName, QnRequestHeaderList(), params, QByteArray(), target, slot, connectionType);
}


// -------------------------------------------------------------------------- //
// QnSessionManager :: handlers
// -------------------------------------------------------------------------- //
void QnSessionManager::at_aboutToBeStarted() {
    assert(QThread::currentThread() == this->thread());

    QMutexLocker locker(&m_accessManagerMutex);
    if (m_accessManager)
        return;

    m_accessManager = new QNetworkAccessManager(this);
}

void QnSessionManager::at_aboutToBeStopped() {
    assert(QThread::currentThread() == this->thread());

    QMutexLocker locker(&m_accessManagerMutex);
    if (!m_accessManager)
        return;

    /* Note that this object is still access manager's parent, 
     * so access manager will be deleted even if deleteLater is not delivered. */
    m_accessManager->deleteLater();
    m_accessManager = 0;
}

void QnSessionManager::at_asyncRequestQueued(int operation, QnSessionManagerAsyncReplyProcessor* replyProcessor, const QUrl& url, const QString &objectName, const QnRequestHeaderList &headers, const QnRequestParamList &params, const QByteArray& data) {
    assert(QThread::currentThread() == this->thread());

    if (!m_accessManager) {
        qWarning() << "doSendAsyncGetRequest is called, while accessManager = 0";
        return;
    }

    QNetworkRequest request;
    request.setUrl(createApiUrl(url, objectName, params));

    foreach (QnRequestHeader header, headers)
        request.setRawHeader(header.first.toAscii(), header.second.toUtf8());

    request.setRawHeader("Authorization", "Basic " + url.userInfo().toLatin1().toBase64());
    request.setHeader(QNetworkRequest::ContentTypeHeader, QLatin1String("text/xml"));

    QNetworkReply* reply = NULL;
    switch(operation) {
    case QNetworkAccessManager::GetOperation:
        reply = m_accessManager->get(request);
        break;
    case QNetworkAccessManager::PostOperation:
        reply = m_accessManager->post(request, data);
        break;
    case QNetworkAccessManager::DeleteOperation:
        reply = m_accessManager->deleteResource(request);
        break;
    default:
        qnWarning("Unknown HTTP operation '%1'.", static_cast<int>(operation));
        break;
    }

    connect(reply, SIGNAL(finished()), replyProcessor, SLOT(at_replyReceived()));
    connect(reply, SIGNAL(sslErrors(QList<QSslError>)), reply, SLOT(ignoreSslErrors()));
}

void QnSessionManager::at_replyProcessor_finished(const QnHTTPRawResponse& response, int) {
    emit replyReceived(response.status);
}
