#include "session_manager.h"

#include <iostream>

#include <QtCore/QMetaEnum>
#include <QtCore/QUrl>
#include <QtCore/QBuffer>
#include <QtCore/QScopedPointer>
#include <QtCore/QUrlQuery>

#include <nx/network/http/http_types.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/future.h>

#include "utils/common/warnings.h"
#include "utils/common/delete_later.h"
#include "utils/common/object_thread_puller.h"
#include "common/common_module.h"
#include "app_server_connection.h"
#include <api/network_proxy_factory.h>
#include <nx/network/http/custom_headers.h>
#include "http_client_pool.h"
#include <nx_ec/ec_api.h>

// -------------------------------------------------------------------------- //
// QnSessionManager
// -------------------------------------------------------------------------- //

QnSessionManager::QnSessionManager(QObject* parent):
    QObject(parent),
    QnCommonModuleAware(parent)
{
    qRegisterMetaType<AsyncRequestInfo>();

    auto httpPool = nx_http::ClientPool::instance();
    Qn::directConnect(
        httpPool, &nx_http::ClientPool::done,
        this, &QnSessionManager::onHttpClientDone);
}

QnSessionManager::~QnSessionManager()
{
    disconnect(this, NULL, this, NULL);
    directDisconnectAll();

    QnMutexLocker lk(&m_mutex);
    auto requestInProgress = std::move(m_requestInProgress);
    lk.unlock();

    auto httpPool = nx_http::ClientPool::instance();
    for (auto& httpClientAndRequestInfo : requestInProgress)
        httpPool->terminate(httpClientAndRequestInfo.first);
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
    const nx::utils::Url& url,
    const QString &objectName,
    nx_http::HttpHeaders headers,
    const QnRequestParamList &params,
    QByteArray msgBody,
    QnHTTPRawResponse &response)
{
    AsyncRequestInfo reqInfo;
    nx::utils::promise<QnHTTPRawResponse> requestedCompletedPromise;
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

int QnSessionManager::sendSyncGetRequest(const nx::utils::Url& url, const QString &objectName, QnHTTPRawResponse& response) {
    return sendSyncGetRequest(url, objectName, nx_http::HttpHeaders(), QnRequestParamList(), response);
}

int QnSessionManager::sendSyncGetRequest(const nx::utils::Url& url, const QString &objectName, nx_http::HttpHeaders headers, const QnRequestParamList &params, QnHTTPRawResponse& response) {
    return sendSyncRequest(nx_http::Method::get, url, objectName, std::move(headers), params, QByteArray(), response);
}

int QnSessionManager::sendSyncPostRequest(const nx::utils::Url& url, const QString &objectName, QByteArray msgBody, QnHTTPRawResponse& response) {
    return sendSyncPostRequest(url, objectName, nx_http::HttpHeaders(), QnRequestParamList(), std::move(msgBody), response);
}

int QnSessionManager::sendSyncPostRequest(const nx::utils::Url& url, const QString &objectName, nx_http::HttpHeaders headers, const QnRequestParamList &params, QByteArray msgBody, QnHTTPRawResponse& response) {
    return sendSyncRequest(nx_http::Method::post, url, objectName, std::move(headers), params, std::move(msgBody), response);
}


// -------------------------------------------------------------------------- //
// QnSessionManager :: async API
// -------------------------------------------------------------------------- //
int QnSessionManager::sendAsyncRequest(
    nx_http::Method::ValueType method,
    const nx::utils::Url& url,
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

int QnSessionManager::sendAsyncGetRequest(const nx::utils::Url& url, const QString &objectName, QObject *target, const char *slot, Qt::ConnectionType connectionType) {
    return sendAsyncGetRequest(url, objectName, nx_http::HttpHeaders(), QnRequestParamList(), target, slot, connectionType);
}

int QnSessionManager::sendAsyncGetRequest(const nx::utils::Url& url, const QString &objectName, nx_http::HttpHeaders headers, const QnRequestParamList &params, QObject *target, const char *slot, Qt::ConnectionType connectionType)
{
    return sendAsyncRequest(nx_http::Method::get, url, objectName, std::move(headers), params, QByteArray(), target, slot, connectionType);
}

int QnSessionManager::sendAsyncPostRequest(const nx::utils::Url& url, const QString &objectName, QByteArray msgBody, QObject *target, const char *slot, Qt::ConnectionType connectionType) {
    return sendAsyncPostRequest(url, objectName, nx_http::HttpHeaders(), QnRequestParamList(), std::move(msgBody), target, slot, connectionType);
}

int QnSessionManager::sendAsyncPostRequest(const nx::utils::Url& url, const QString &objectName, nx_http::HttpHeaders headers, const QnRequestParamList &params, QByteArray msgBody, QObject *target,
                                           const char *slot, Qt::ConnectionType connectionType)
{
    return sendAsyncRequest(nx_http::Method::post, url, objectName, std::move(headers), params, std::move(msgBody), target, slot, connectionType);
}


nx::utils::Url QnSessionManager::createApiUrl(const nx::utils::Url& baseUrl, const QString &objectName, const QnRequestParamList &params) const
{
    nx::utils::Url url(baseUrl);

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
    const nx::utils::Url& _url,
    const QString &objectName,
    nx_http::HttpHeaders headers,
    const QnRequestParamList &params,
    const QByteArray& msgBody,
    AsyncRequestInfo requestInfo)
{
    nx::utils::Url url(_url);
    auto proxyList = QNetworkProxyFactory::proxyForQuery(QNetworkProxyQuery(url.toQUrl()));
    if (!proxyList.isEmpty())
    {
        QNetworkProxy proxy = proxyList[0];
        if (proxy.type() == QNetworkProxy::HttpProxy)
        {
            url.setHost(proxy.hostName());
            url.setPort(proxy.port());
            url.setUserName(proxy.user());
            url.setPassword(proxy.password());
        }
        else if (proxy.type() != QNetworkProxy::NoProxy)
        {
            NX_ASSERT(0, Q_FUNC_INFO, "Not implemented!");
            return -1;
        }
    }

    if (method != nx_http::Method::get &&
        method != nx_http::Method::post)
    {
        NX_ASSERT(false, Q_FUNC_INFO,
            lit("Unknown HTTP operation '%1'.").arg(QString::fromLatin1(method)));
        return -1;
    }

    const auto& connection = commonModule()->ec2Connection();
    if (!connection)
    {
        NX_ASSERT(false, Q_FUNC_INFO,
            lit("Not connected to ec2 database."));
        return -1;
    }

    const nx::utils::Url appServerUrl = connection->connectionInfo().ecUrl;
    auto requestUrl = createApiUrl(url, objectName, params);
    if (!requestUrl.isValid() || url.host().isEmpty())
        return -1;
    requestUrl.setUserName(_url.userName().isEmpty() ? appServerUrl.userName() : _url.userName());
    requestUrl.setPassword(_url.password().isEmpty() ? appServerUrl.password() : _url.password());

    auto httpPool = nx_http::ClientPool::instance();

    const auto msgBodyContentType =
        nx_http::getHeaderValue(headers, nx_http::header::kContentType);
    headers.emplace(Qn::CUSTOM_CHANGE_REALM_HEADER_NAME, QByteArray());

    if (method == nx_http::Method::get)
        requestInfo.handle = httpPool->doGet(requestUrl, std::move(headers));
    else
        requestInfo.handle = httpPool->doPost(requestUrl, msgBodyContentType, msgBody, std::move(headers));

    QnMutexLocker lk(&m_mutex);
    m_requestInProgress.emplace(requestInfo.handle, requestInfo);

    return requestInfo.handle;
}

void QnSessionManager::onHttpClientDone(int requestId, nx_http::AsyncHttpClientPtr clientPtr)
{
    AsyncRequestInfo requestInfo;
    {
        QnMutexLocker lk(&m_mutex);
        auto it = m_requestInProgress.find(requestId);
        if (it == m_requestInProgress.end())
            return; //request has been cancelled?
        requestInfo = std::move(it->second);
        m_requestInProgress.erase(it);
    }

    QnHTTPRawResponse httpRequestResult(
        ((clientPtr->lastSysErrorCode() == SystemError::noError) && clientPtr->failed())
            ? SystemError::connectionReset
            : clientPtr->lastSysErrorCode(),
        clientPtr->response() ? clientPtr->response()->statusLine : nx_http::StatusLine(),
        clientPtr->contentType(),
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
