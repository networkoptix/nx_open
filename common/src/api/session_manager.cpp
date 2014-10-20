#include "session_manager.h"

#include <cassert>

#include <QtCore/QMetaEnum>
#include <QtCore/QUrl>
#include <QtCore/QBuffer>
#include <QtCore/QThread>
#include <QtCore/QScopedPointer>
#include <QtCore/QUrlQuery>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkAccessManager>

#include "utils/common/log.h"
#include "utils/common/warnings.h"
#include "utils/common/delete_later.h"
#include "utils/common/object_thread_puller.h"
#include "common/common_module.h"
#include "app_server_connection.h"

#include <iostream>

int QnSessionManagerSyncReply::wait(QnHTTPRawResponse& response) {
    QMutexLocker locker(&m_mutex);
    while (!m_finished)
        m_condition.wait(&m_mutex);

    response = m_response;
    return m_response.status;
}

void QnSessionManagerSyncReply::terminate() {
    QMutexLocker locker(&m_mutex);
    m_finished = true;
    m_condition.wakeOne();
}

void QnSessionManagerSyncReply::requestFinished(const QnHTTPRawResponse& response, int handle) 
{
    Q_UNUSED(handle)
    QMutexLocker locker(&m_mutex);
    m_response = response;
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
    qRegisterMetaType<AsyncRequestInfo>();

    connect(this, SIGNAL(asyncRequestQueued(int, AsyncRequestInfo, QUrl, QString, QnRequestHeaderList, QnRequestParamList, QByteArray)), 
            this, SLOT(at_asyncRequestQueued(int, AsyncRequestInfo, QUrl, QString, QnRequestHeaderList, QnRequestParamList, QByteArray)));
    connect(this, SIGNAL(aboutToBeStopped()), this, SLOT(at_aboutToBeStopped()));
    connect(this, SIGNAL(aboutToBeStarted()), this, SLOT(at_aboutToBeStarted()));

    m_thread->setObjectName( QLatin1String("QnSessionManagerThread") ); /* Name will be shown in debugger. */
    Q_ASSERT(parent == 0);
    this->moveToThread(m_thread.data());

    m_thread->start();

    // SessionManager is not calculation intensive, but needs to be responsible
    m_thread->setPriority(QThread::HighestPriority);
}

QnSessionManager::~QnSessionManager() {
    disconnect(this, NULL, this, NULL);

    {
        QMutexLocker lock(&m_syncReplyMutex);
        foreach(QnSessionManagerSyncReply* syncReply, m_syncReplyInProgress)
            syncReply->terminate();
    }

    QScopedPointer<QnObjectThreadPuller> puller(new QnObjectThreadPuller());
    puller->pull(this);
    puller->wait(this);

    m_thread->exit();
    m_thread->wait();

    at_aboutToBeStopped();
}

void QnSessionManager::at_replyReceived(QNetworkReply * reply) 
{
    QString errorString = reply->errorString();
    // Common Server error looks like:
    // "Error downloading https://user:password@host:port/path - server replied: INTERNAL SERVER ERROR"
    // displaying plain-text password is unsecure and strongly not recommended
    if (errorString.indexOf(QLatin1String("@")) > 0 && errorString.indexOf(QLatin1String(":")) > 0) {
        int n = errorString.lastIndexOf(QLatin1String(":"));
        errorString = errorString.mid(n + 1).trimmed();
    }

    AsyncRequestInfo reqInfo = m_handleInProgress.value(reply);
    m_handleInProgress.remove(reply);
    //emit requestFinished(QnHTTPRawResponse(reply->error(), reply->rawHeaderPairs(), reply->readAll(), errorString.toLatin1()), handle);
    QnHTTPRawResponse httpResponse(reply->error(), reply->rawHeaderPairs(), reply->readAll(), errorString.toLatin1());
    if( reqInfo.object )
        QMetaObject::invokeMethod(reqInfo.object, reqInfo.slot, reqInfo.connectionType,
                                  QGenericReturnArgument(), 
                                  QGenericArgument("QnHTTPRawResponse", &httpResponse),
                                  QGenericArgument("int", &reqInfo.handle));
    qnDeleteLater(reply);
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

    QString path = QLatin1String("/api/") + objectName + QLatin1Char('/');
    url.setPath(path);

    QUrlQuery urlQuery(url.query());
    for (int i = 0; i < params.count(); i++){
        QPair<QString, QString> param = params[i];
        urlQuery.addQueryItem(param.first, param.second);
    }
    url.setQuery( urlQuery );
    return url;
}


// -------------------------------------------------------------------------- //
// QnSessionManager :: sync API
// -------------------------------------------------------------------------- //
int QnSessionManager::sendSyncRequest(int operation, const QUrl& url, const QString &objectName, const QnRequestHeaderList &headers, const QnRequestParamList &params, const QByteArray &data, QnHTTPRawResponse &response) 
{

    AsyncRequestInfo reqInfo;
    reqInfo.handle = s_handle.fetchAndAddAcquire(1);
    reqInfo.object = this;
    reqInfo.slot   = "at_SyncRequestFinished";
    reqInfo.connectionType = Qt::AutoConnection;

    QnSessionManagerSyncReply syncReply;
    m_syncReplyMutex.lock();
    m_syncReplyInProgress.insert(reqInfo.handle, &syncReply);
    m_syncReplyMutex.unlock();

    emit asyncRequestQueued(operation, reqInfo, url, objectName, headers, params, data);

    int result = syncReply.wait(response);
   
    QMutexLocker lock(&m_syncReplyMutex);
    m_syncReplyInProgress.remove(reqInfo.handle);
    
    return result;
}

void QnSessionManager::at_SyncRequestFinished(const QnHTTPRawResponse& response, int handle) 
{
    QMutexLocker lock(&m_syncReplyMutex);
    QnSessionManagerSyncReply* reply = m_syncReplyInProgress.value(handle);
    assert(reply);
    if (reply)
        reply->requestFinished(response, handle);
}


int QnSessionManager::sendSyncGetRequest(const QUrl& url, const QString &objectName, QnHTTPRawResponse& response) {
    return sendSyncGetRequest(url, objectName, QnRequestHeaderList(), QnRequestParamList(), response);
}

int QnSessionManager::sendSyncGetRequest(const QUrl& url, const QString &objectName, const QnRequestHeaderList &headers, const QnRequestParamList &params, QnHTTPRawResponse& response) {
    return sendSyncRequest(QNetworkAccessManager::GetOperation, url, objectName, headers, params, QByteArray(), response);
}

int QnSessionManager::sendSyncPostRequest(const QUrl& url, const QString &objectName, const QByteArray& data, QnHTTPRawResponse& response) {
    return sendSyncPostRequest(url, objectName, QnRequestHeaderList(), QnRequestParamList(), data, response);
}

int QnSessionManager::sendSyncPostRequest(const QUrl& url, const QString &objectName, const QnRequestHeaderList &headers, const QnRequestParamList &params, const QByteArray& data, QnHTTPRawResponse& response) {
    return sendSyncRequest(QNetworkAccessManager::PostOperation, url, objectName, headers, params, data, response);
}


// -------------------------------------------------------------------------- //
// QnSessionManager :: async API
// -------------------------------------------------------------------------- //
int QnSessionManager::sendAsyncRequest(int operation, const QUrl& url, const QString &objectName, const QnRequestHeaderList &headers, const QnRequestParamList &params, const QByteArray& data, QObject *target, const char *slot, Qt::ConnectionType connectionType) 
{
    //if you want receive response make sure you have event loop in calling thread
    Q_ASSERT( qnHasEventLoop(QThread::currentThread()) || (!target) );
    if( !qnHasEventLoop(QThread::currentThread()) && target )
    {
        NX_LOG( QString::fromLatin1("QnSessionManager::sendAsyncRequest. No event loop in current thread, "
            "but response is awaited. target %1, slot %2").arg(target->objectName()).arg(QLatin1String(slot)), cl_logERROR );
        std::cout<<"QnSessionManager::sendAsyncRequest. No event loop in current thread, "
            "but response is awaited. target "<<target<<", slot "<<slot<<std::endl;
    }

    AsyncRequestInfo reqInfo;
    reqInfo.handle = s_handle.fetchAndAddAcquire(1);
    reqInfo.object = target;
    reqInfo.slot = slot;
    reqInfo.connectionType = connectionType;
   // qDebug() << "send async request to" << url << data << params;
    emit asyncRequestQueued(operation, reqInfo, url, objectName, headers, params, data);
    return reqInfo.handle;
}

int QnSessionManager::sendAsyncGetRequest(const QUrl& url, const QString &objectName, QObject *target, const char *slot, Qt::ConnectionType connectionType) {
    return sendAsyncGetRequest(url, objectName, QnRequestHeaderList(), QnRequestParamList(), target, slot, connectionType);
}

int QnSessionManager::sendAsyncGetRequest(const QUrl& url, const QString &objectName, const QnRequestHeaderList &headers, const QnRequestParamList &params, QObject *target, const char *slot, Qt::ConnectionType connectionType) 
{
    return sendAsyncRequest(QNetworkAccessManager::GetOperation, url, objectName, headers, params, QByteArray(), target, slot, connectionType);
}

int QnSessionManager::sendAsyncPostRequest(const QUrl& url, const QString &objectName, const QByteArray& data, QObject *target, const char *slot, Qt::ConnectionType connectionType) {
    return sendAsyncPostRequest(url, objectName, QnRequestHeaderList(), QnRequestParamList(), data, target, slot, connectionType);
}

int QnSessionManager::sendAsyncPostRequest(const QUrl& url, const QString &objectName, const QnRequestHeaderList &headers, const QnRequestParamList &params, const QByteArray& data, QObject *target, 
                                           const char *slot, Qt::ConnectionType connectionType) 
{
    return sendAsyncRequest(QNetworkAccessManager::PostOperation, url, objectName, headers, params, data, target, slot, connectionType);
}

int QnSessionManager::sendAsyncDeleteRequest(const QUrl& url, const QString &objectName, int id, QObject *target, const char *slot, Qt::ConnectionType connectionType) {
    QnRequestParamList params;
    params.push_back(QnRequestParam("id", QString::number(id)));
    return sendAsyncDeleteRequest(url, objectName, QnRequestHeaderList(), params, target, slot, connectionType);
}

int QnSessionManager::sendAsyncDeleteRequest(const QUrl& url, const QString &objectName, const QnRequestHeaderList &headers, const QnRequestParamList &params, QObject *target, 
                                             const char *slot, Qt::ConnectionType connectionType) 
{
    return sendAsyncRequest(QNetworkAccessManager::DeleteOperation, url, objectName, headers, params, QByteArray(), target, slot, connectionType);
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

    //m_accessManager->moveToThread(m_thread.data());
    connect(m_accessManager, SIGNAL(authenticationRequired(QNetworkReply*, QAuthenticator *)), this, SLOT(at_authenticationRequired(QNetworkReply*, QAuthenticator *)), Qt::DirectConnection);
    connect(m_accessManager, SIGNAL(proxyAuthenticationRequired(const QNetworkProxy&, QAuthenticator*)), this, SLOT(at_proxyAuthenticationRequired(const QNetworkProxy&, QAuthenticator*)), Qt::DirectConnection);
    connect(m_accessManager, SIGNAL(sslErrors(QNetworkReply*,QList<QSslError>)), this, SLOT(at_sslErrors(QNetworkReply*,QList<QSslError>)));
    connect(m_accessManager, SIGNAL(finished(QNetworkReply *)), this, SLOT(at_replyReceived(QNetworkReply *)));
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

void QnSessionManager::at_proxyAuthenticationRequired ( const QNetworkProxy &, QAuthenticator* authenticator)
{
    QString user = QnAppServerConnectionFactory::url().userName();
    QString password = QnAppServerConnectionFactory::url().password();
    authenticator->setUser(user);
    authenticator->setPassword(password);
}

void QnSessionManager::at_authenticationRequired(QNetworkReply* reply, QAuthenticator * authenticator)
{
    // QnSessionManager instance can be used with different instances of QnAppServerConnection simultaneously
    // so we first checking the reply url for login and password - they are present in case of Server connections
    // otherwise - mediaserver connections do not include login and password in the url, but we can have
    // mediaserver connections only within one Server session so we can use defaultUrl() method

    bool useDefaultValues = reply->url().userName().isEmpty();
    QString user = useDefaultValues
            ? QnAppServerConnectionFactory::url().userName()
            :reply->url().userName();
    QString password = useDefaultValues
            ? QnAppServerConnectionFactory::url().password()
            : reply->url().password();

    // if current values are already present in authenticator, do not send them again -
    // it will cause an infinite loop until a timeout
    if ((authenticator->user() == user && authenticator->password() == password))
        return;

    authenticator->setUser(user.toLower());
    authenticator->setPassword(password);
}

void QnSessionManager::at_asyncRequestQueued(int operation, AsyncRequestInfo reqInfo, const QUrl& url, const QString &objectName, const QnRequestHeaderList &headers, 
                                             const QnRequestParamList &params, const QByteArray& data) 
{
    assert(QThread::currentThread() == this->thread());

    {
        QMutexLocker lock(&m_accessManagerMutex);
        if (!m_accessManager) {
            qWarning() << "doSendAsyncGetRequest is called, while accessManager = 0";
            return;
        }
    }

    QNetworkRequest request;
    request.setUrl(createApiUrl(url, objectName, params));

    //qDebug() << "api url" << request.url();

    bool skipContentType = false;
    foreach (const QnRequestHeader& header, headers) {
        if (header.first == QLatin1String("Content-Type"))
            skipContentType = true;
        request.setRawHeader(header.first.toLatin1(), header.second.toUtf8());
    }

    QString userInfo = url.userInfo();
    if (!userInfo.isEmpty())
        request.setRawHeader("Authorization", "Basic " + userInfo.toLatin1().toBase64());
    if (!skipContentType)
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
        return;
    }
    m_handleInProgress.insert(reply, reqInfo);
}

void QnSessionManager::at_sslErrors(QNetworkReply* reply, const QList<QSslError> &)
{
    reply->ignoreSslErrors();
}
