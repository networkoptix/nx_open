
#include "session_manager.h"

#include <iostream>

#include <QtCore/QMetaEnum>
#include <QtCore/QUrl>
#include <QtCore/QBuffer>
#include <QtCore/QScopedPointer>
#include <QtCore/QUrlQuery>

#include <nx/network/http/httptypes.h>
#include <nx/utils/log/log.h>

#include "utils/common/warnings.h"
#include "utils/common/delete_later.h"
#include "utils/common/object_thread_puller.h"
#include "common/common_module.h"
#include "app_server_connection.h"


// -------------------------------------------------------------------------- //
// QnSessionManager
// -------------------------------------------------------------------------- //
QAtomicInt QnSessionManager::s_handle(1);

QnSessionManager::QnSessionManager(QObject *parent)
: 
    QObject(parent)
{
    qRegisterMetaType<AsyncRequestInfo>();
}

QnSessionManager::~QnSessionManager()
{
    disconnect(this, NULL, this, NULL);

    QnMutexLocker lk(&m_mutex);
    auto requestInProgress = std::move(m_requestInProgress);
    lk.unlock();

    for (auto& httpClientAndRequestInfo: requestInProgress)
        httpClientAndRequestInfo.first->terminate();
}

QnSessionManager *QnSessionManager::instance()
{
    NX_ASSERT(qnCommon->instance<QnSessionManager>(), Q_FUNC_INFO, "Make sure session manager exists");
    return qnCommon->instance<QnSessionManager>();
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
    QnMutexLocker locker( &m_mutex );
    return true;
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


// -------------------------------------------------------------------------- //
// QnSessionManager :: sync API
// -------------------------------------------------------------------------- //
QNetworkReply::NetworkError QnSessionManager::sendSyncRequest(
    nx_http::Method::ValueType method,
    const QUrl& url,
    const QString &objectName,
    nx_http::HttpHeaders headers,
    const QnRequestParamList &params,
    QByteArray msgBody,
    QnHTTPRawResponse &response)
{
    AsyncRequestInfo reqInfo;
    reqInfo.handle = s_handle.fetchAndAddAcquire(1);
    std::promise<QnHTTPRawResponse> requestedCompletedPromise;
    reqInfo.requestedCompletedPromise = &requestedCompletedPromise;

    sendAsyncRequest(
        method,
        url,
        objectName,
        std::move(headers),
        params,
        std::move(msgBody),
        std::move(reqInfo));

    response = requestedCompletedPromise.get_future().get();
    return response.status;
}

int QnSessionManager::sendSyncGetRequest(const QUrl& url, const QString &objectName, QnHTTPRawResponse& response) {
    return sendSyncGetRequest(url, objectName, nx_http::HttpHeaders(), QnRequestParamList(), response);
}

int QnSessionManager::sendSyncGetRequest(const QUrl& url, const QString &objectName, nx_http::HttpHeaders headers, const QnRequestParamList &params, QnHTTPRawResponse& response) {
    return sendSyncRequest(nx_http::Method::GET, url, objectName, std::move(headers), params, QByteArray(), response);
}

int QnSessionManager::sendSyncPostRequest(const QUrl& url, const QString &objectName, QByteArray msgBody, QnHTTPRawResponse& response) {
    return sendSyncPostRequest(url, objectName, nx_http::HttpHeaders(), QnRequestParamList(), std::move(msgBody), response);
}

int QnSessionManager::sendSyncPostRequest(const QUrl& url, const QString &objectName, nx_http::HttpHeaders headers, const QnRequestParamList &params, QByteArray msgBody, QnHTTPRawResponse& response) {
    return sendSyncRequest(nx_http::Method::POST, url, objectName, std::move(headers), params, std::move(msgBody), response);
}


// -------------------------------------------------------------------------- //
// QnSessionManager :: async API
// -------------------------------------------------------------------------- //
int QnSessionManager::sendAsyncRequest(
    nx_http::Method::ValueType method,
    const QUrl& url,
    const QString &objectName,
    nx_http::HttpHeaders headers,
    const QnRequestParamList &params,
    QByteArray msgBody,
    QObject *target,
    const char *slot,
    Qt::ConnectionType connectionType)
{
    //if you want receive response make sure you have event loop in calling thread
    NX_ASSERT(qnHasEventLoop(QThread::currentThread()) || (!target));
    if (!qnHasEventLoop(QThread::currentThread()) && target)
    {
        NX_LOG(QString::fromLatin1("QnSessionManager::sendAsyncRequest. No event loop in current thread, "
            "but response is awaited. target %1, slot %2").arg(target->objectName()).arg(QLatin1String(slot)), cl_logERROR);
        std::cout << "QnSessionManager::sendAsyncRequest. No event loop in current thread, "
            "but response is awaited. target " << target << ", slot " << slot << std::endl;
    }

    AsyncRequestInfo reqInfo;
    reqInfo.handle = s_handle.fetchAndAddAcquire(1);
    reqInfo.object = target;
    reqInfo.slot = slot;
    reqInfo.connectionType = connectionType;

    return sendAsyncRequest(
        method,
        url,
        objectName,
        std::move(headers),
        params,
        std::move(msgBody),
        std::move(reqInfo));
}

int QnSessionManager::sendAsyncGetRequest(const QUrl& url, const QString &objectName, QObject *target, const char *slot, Qt::ConnectionType connectionType) {
    return sendAsyncGetRequest(url, objectName, nx_http::HttpHeaders(), QnRequestParamList(), target, slot, connectionType);
}

int QnSessionManager::sendAsyncGetRequest(const QUrl& url, const QString &objectName, nx_http::HttpHeaders headers, const QnRequestParamList &params, QObject *target, const char *slot, Qt::ConnectionType connectionType)
{
    return sendAsyncRequest(nx_http::Method::GET, url, objectName, std::move(headers), params, QByteArray(), target, slot, connectionType);
}

int QnSessionManager::sendAsyncPostRequest(const QUrl& url, const QString &objectName, QByteArray msgBody, QObject *target, const char *slot, Qt::ConnectionType connectionType) {
    return sendAsyncPostRequest(url, objectName, nx_http::HttpHeaders(), QnRequestParamList(), std::move(msgBody), target, slot, connectionType);
}

int QnSessionManager::sendAsyncPostRequest(const QUrl& url, const QString &objectName, nx_http::HttpHeaders headers, const QnRequestParamList &params, QByteArray msgBody, QObject *target,
                                           const char *slot, Qt::ConnectionType connectionType) 
{
    return sendAsyncRequest(nx_http::Method::POST, url, objectName, std::move(headers), params, std::move(msgBody), target, slot, connectionType);
}


QUrl QnSessionManager::createApiUrl(const QUrl& baseUrl, const QString &objectName, const QnRequestParamList &params) const
{
    QUrl url(baseUrl);

    QString path = objectName + L'/';
    if (!objectName.startsWith(lit("ec2/")))
        path = lit("/api/") + path;
    else
        path = L'/' + path;
    url.setPath(path);

    QUrlQuery urlQuery(url.query());
    for (int i = 0; i < params.count(); i++)
    {
        QPair<QString, QString> param = params[i];
        urlQuery.addQueryItem(param.first, param.second);
    }
    url.setQuery(urlQuery);
    return url;
}

int QnSessionManager::sendAsyncRequest(
    nx_http::Method::ValueType method,
    const QUrl& url,
    const QString &objectName,
    nx_http::HttpHeaders headers,
    const QnRequestParamList &params,
    const QByteArray& msgBody,
    AsyncRequestInfo requestInfo)
{
    auto clientPtr = nx_http::AsyncHttpClient::create();
    //TODO #ak set appropriate timeouts

    const auto requestUrl = createApiUrl(url, objectName, params);
    const auto appServerUrl = QnAppServerConnectionFactory::url();
    clientPtr->setUserName(appServerUrl.userName());
    clientPtr->setUserPassword(appServerUrl.password());

    const auto msgBodyContentType = nx_http::getHeaderValue(headers, "Content-Type");
    clientPtr->setAdditionalHeaders(std::move(headers));

    connect(
        clientPtr.get(), &nx_http::AsyncHttpClient::done,
        this, &QnSessionManager::onHttpClientDone,
        Qt::DirectConnection);

    {
        QnMutexLocker lk(&m_mutex);
        m_requestInProgress.emplace(clientPtr, requestInfo);
    }

    if (method == nx_http::Method::GET)
    {
        clientPtr->doGet(requestUrl);
    }
    else if (method == nx_http::Method::POST)
    {
        clientPtr->doPost(requestUrl, msgBodyContentType, msgBody);
    }
    else
    {
        NX_LOG(lit("Unknown HTTP operation '%1'.")
            .arg(QString::fromLatin1(method)), cl_logWARNING);
        return -1;
    }

    return requestInfo.handle;
}

void QnSessionManager::onHttpClientDone(nx_http::AsyncHttpClientPtr clientPtr)
{
    AsyncRequestInfo requestInfo;
    {
        QnMutexLocker lk(&m_mutex);
        auto it = m_requestInProgress.find(clientPtr);
        if (it == m_requestInProgress.end())
            return; //request has been cancelled?
        requestInfo = std::move(it->second);
        m_requestInProgress.erase(it);
    }

    QnHTTPRawResponse httpRequestResult(
        clientPtr->lastSysErrorCode(),
        clientPtr->response() ? *clientPtr->response() : nx_http::Response(),
        clientPtr->fetchMessageBodyBuffer());

    if (requestInfo.object)
    {
        QMetaObject::invokeMethod(
            requestInfo.object, requestInfo.slot, requestInfo.connectionType,
            QGenericReturnArgument(),
            Q_ARG(QnHTTPRawResponse, httpRequestResult),
            Q_ARG(int, requestInfo.handle));
    }
    else if (requestInfo.requestedCompletedPromise)
    {
        requestInfo.requestedCompletedPromise->set_value(std::move(httpRequestResult));
    }
}
