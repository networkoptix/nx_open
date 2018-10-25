#include "asynchttpclient.h"

#include <condition_variable>
#include <mutex>

#include <QtCore/QCryptographicHash>
#include <QtCore/QDateTime>

#include <nx/network/url/url_parse_helper.h>
#include <nx/network/socket_factory.h>
#include <nx/network/socket_global.h>
#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>

#include <nx/utils/crypt/linux_passwd_crypt.h>
#include <nx/utils/system_error.h>

#include "nx/network/http/buffer_source.h"
#include "nx/network/http/custom_headers.h"

using std::make_pair;

namespace nx {
namespace network {
namespace http {

AsyncHttpClient::AsyncHttpClient()
{
    initDelegate();
}

AsyncHttpClient::AsyncHttpClient(std::unique_ptr<AbstractStreamSocket> socket):
    m_delegate(std::move(socket))
{
    initDelegate();
}

void AsyncHttpClient::initDelegate()
{
    using namespace std::placeholders;

    m_delegate.setOnRequestHasBeenSent(
        std::bind(&AsyncHttpClient::onRequestHasBeenSent, this, _1));
    m_delegate.setOnResponseReceived(
        std::bind(&AsyncHttpClient::onResponseReceived, this));
    m_delegate.setOnSomeMessageBodyAvailable(
        std::bind(&AsyncHttpClient::onSomeMessageBodyAvailable, this));
    m_delegate.setOnDone(
        std::bind(&AsyncHttpClient::onDone, this));
}

AsyncHttpClient::~AsyncHttpClient()
{
    pleaseStopSync();
}

void AsyncHttpClient::pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    m_delegate.pleaseStop(std::move(completionHandler));
}

void AsyncHttpClient::pleaseStopSync()
{
    m_delegate.pleaseStopSync();
}

nx::network::aio::AbstractAioThread* AsyncHttpClient::getAioThread() const
{
    return m_delegate.getAioThread();
}

void AsyncHttpClient::bindToAioThread(nx::network::aio::AbstractAioThread* aioThread)
{
    m_delegate.bindToAioThread(aioThread);
}

void AsyncHttpClient::post(nx::utils::MoveOnlyFunc<void()> func)
{
    m_delegate.post(std::move(func));
}

void AsyncHttpClient::dispatch(nx::utils::MoveOnlyFunc<void()> func)
{
    m_delegate.dispatch(std::move(func));
}

AsyncClient::State AsyncHttpClient::state() const
{
    return m_delegate.state();
}

bool AsyncHttpClient::failed() const
{
    return m_delegate.failed();
}

SystemError::ErrorCode AsyncHttpClient::lastSysErrorCode() const
{
    return m_delegate.lastSysErrorCode();
}

void AsyncHttpClient::doGet(const nx::utils::Url& url)
{
    m_delegate.doGet(url);
}

void AsyncHttpClient::doGet(
    const nx::utils::Url& url,
    nx::utils::MoveOnlyFunc<void(AsyncHttpClientPtr)> completionHandler)
{
    m_onDoneHandler = std::move(completionHandler);
    m_delegate.doGet(url);
}

void AsyncHttpClient::doHead(const nx::utils::Url& url)
{
    m_delegate.doHead(url);
}

void AsyncHttpClient::doHead(
    const nx::utils::Url& url,
    nx::utils::MoveOnlyFunc<void(AsyncHttpClientPtr)> completionHandler)
{
    m_onDoneHandler = std::move(completionHandler);
    m_delegate.doHead(url);
}

namespace {

class CustomBufferSource:
    public BufferSource
{
    using base_type = BufferSource;

public:
    CustomBufferSource(
        const StringType& mimeType,
        const BufferType& msgBody,
        bool includeContentLength)
        :
        base_type(mimeType, msgBody),
        m_includeContentLength(includeContentLength)
    {
    }

    virtual boost::optional<uint64_t> contentLength() const override
    {
        if (m_includeContentLength)
            return base_type::contentLength();
        else
            return boost::none;
    }

private:
    bool m_includeContentLength;
};

} // namespace

void AsyncHttpClient::doPost(
    const nx::utils::Url& url,
    const nx::network::http::StringType& contentType,
    nx::network::http::StringType messageBodyBuffer,
    bool includeContentLength)
{
    auto messageBody = std::make_unique<CustomBufferSource>(
        contentType,
        std::move(messageBodyBuffer),
        includeContentLength);
    m_delegate.setRequestBody(std::move(messageBody));
    m_delegate.doPost(url);
}

void AsyncHttpClient::doPost(
    const nx::utils::Url& url,
    const nx::network::http::StringType& contentType,
    nx::network::http::StringType messageBody,
    bool includeContentLength,
    nx::utils::MoveOnlyFunc<void(AsyncHttpClientPtr)> completionHandler)
{
    m_onDoneHandler = std::move(completionHandler);
    doPost(url, contentType, std::move(messageBody), includeContentLength);
}

void AsyncHttpClient::doPut(
    const nx::utils::Url& url,
    const nx::network::http::StringType& contentType,
    nx::network::http::StringType messageBodyBuffer)
{
    auto messageBody = std::make_unique<BufferSource>(
        contentType,
        std::move(messageBodyBuffer));
    m_delegate.setRequestBody(std::move(messageBody));
    m_delegate.doPut(url);
}

void AsyncHttpClient::doPut(
    const nx::utils::Url& url,
    const nx::network::http::StringType& contentType,
    nx::network::http::StringType messageBody,
    nx::utils::MoveOnlyFunc<void(AsyncHttpClientPtr)> completionHandler)
{
    m_onDoneHandler = std::move(completionHandler);
    doPut(url, contentType, std::move(messageBody));
}

void AsyncHttpClient::doDelete(const nx::utils::Url& url)
{
    m_delegate.doDelete(url);
}

void AsyncHttpClient::doDelete(
    const nx::utils::Url& url,
    nx::utils::MoveOnlyFunc<void (AsyncHttpClientPtr)> completionHandler)
{
    m_onDoneHandler = std::move(completionHandler);
    doDelete(url);
}

void AsyncHttpClient::doUpgrade(
    const nx::utils::Url& url,
    const StringType& protocolToUpgradeTo)
{
    m_delegate.doUpgrade(url, protocolToUpgradeTo, [](){});
}

void AsyncHttpClient::doUpgrade(
    const nx::utils::Url& url,
    const StringType& protocolToUpgradeTo,
    nx::utils::MoveOnlyFunc<void(AsyncHttpClientPtr)> completionHandler)
{
    m_onDoneHandler = std::move(completionHandler);
    doUpgrade(url, protocolToUpgradeTo);
}

const nx::network::http::Request& AsyncHttpClient::request() const
{
    return m_delegate.request();
}

const Response* AsyncHttpClient::response() const
{
    return m_delegate.response();
}

StringType AsyncHttpClient::contentType() const
{
    return m_delegate.contentType();
}

bool AsyncHttpClient::hasRequestSucceeded() const
{
    return m_delegate.hasRequestSucceeded();
}

BufferType AsyncHttpClient::fetchMessageBodyBuffer()
{
    return m_delegate.fetchMessageBodyBuffer();
}

const nx::utils::Url& AsyncHttpClient::url() const
{
    return m_delegate.url();
}

const nx::utils::Url& AsyncHttpClient::contentLocationUrl() const
{
    return m_delegate.contentLocationUrl();
}

quint64 AsyncHttpClient::bytesRead() const
{
    return m_delegate.bytesRead();
}

void AsyncHttpClient::setUseCompression(bool toggleUseEntityEncoding)
{
    m_delegate.setUseCompression(toggleUseEntityEncoding);
}

void AsyncHttpClient::setSubsequentReconnectTries(int reconnectTries)
{
    m_delegate.setSubsequentReconnectTries(reconnectTries);
}

void AsyncHttpClient::setTotalReconnectTries(int reconnectTries)
{
    m_delegate.setTotalReconnectTries(reconnectTries);
}

void AsyncHttpClient::setUserAgent(const QString& userAgent)
{
    m_delegate.setUserAgent(userAgent);
}

void AsyncHttpClient::setUserName(const QString& userName)
{
    m_delegate.setUserName(userName);
}

void AsyncHttpClient::setUserPassword(const QString& userPassword)
{
    m_delegate.setUserPassword(userPassword);
}

void AsyncHttpClient::setProxyUserName(const QString& userName)
{
    m_delegate.setProxyUserName(userName);
}

void AsyncHttpClient::setProxyUserPassword(const QString& userPassword)
{
    m_delegate.setProxyUserPassword(userPassword);
}

void AsyncHttpClient::setAuth(const AuthInfo& auth)
{
    m_delegate.setAuth(auth);
}

void AsyncHttpClient::setUserAuthToken(const AuthToken& userToken)
{
    m_delegate.setUserAuthToken(userToken);
}

void AsyncHttpClient::setUserCredentials(const Credentials& userCredentials)
{
    m_delegate.setUserCredentials(userCredentials);
}

void AsyncHttpClient::setProxyUserAuthToken(const AuthToken& userToken)
{
    m_delegate.setProxyUserAuthToken(userToken);
}

void AsyncHttpClient::setProxyUserCredentials(const Credentials& userCredentials)
{
    m_delegate.setProxyUserCredentials(userCredentials);
}

void AsyncHttpClient::setProxyVia(const SocketAddress& proxyEndpoint, bool isSecure)
{
    m_delegate.setProxyVia(proxyEndpoint, isSecure);
}

void AsyncHttpClient::setMaxNumberOfRedirects(int maxNumberOfRedirects)
{
    m_delegate.setMaxNumberOfRedirects(maxNumberOfRedirects);
}

void AsyncHttpClient::setDisablePrecalculatedAuthorization(bool val)
{
    m_delegate.setDisablePrecalculatedAuthorization(val);
}

void AsyncHttpClient::setSendTimeoutMs(unsigned int sendTimeoutMs)
{
    m_delegate.setSendTimeout(std::chrono::milliseconds(sendTimeoutMs));
}

void AsyncHttpClient::setResponseReadTimeoutMs(unsigned int responseReadTimeoutMs)
{
    m_delegate.setResponseReadTimeout(std::chrono::milliseconds(responseReadTimeoutMs));
}

void AsyncHttpClient::setMessageBodyReadTimeoutMs(unsigned int messageBodyReadTimeoutMs)
{
    m_delegate.setMessageBodyReadTimeout(std::chrono::milliseconds(messageBodyReadTimeoutMs));
}

const std::unique_ptr<AbstractStreamSocket>& AsyncHttpClient::socket()
{
    return m_delegate.socket();
}

std::unique_ptr<AbstractStreamSocket> AsyncHttpClient::takeSocket()
{
    return m_delegate.takeSocket();
}

void AsyncHttpClient::addAdditionalHeader(const StringType& key, const StringType& value)
{
    m_delegate.addAdditionalHeader(key, value);
}

void AsyncHttpClient::addRequestHeaders(const HttpHeaders& headers)
{
    m_delegate.addRequestHeaders(headers);
}

void AsyncHttpClient::removeAdditionalHeader(const StringType& key)
{
    m_delegate.removeAdditionalHeader(key);
}

void AsyncHttpClient::setAdditionalHeaders(HttpHeaders additionalHeaders)
{
    m_delegate.setAdditionalHeaders(std::move(additionalHeaders));
}

void AsyncHttpClient::setAuthType(AuthType value)
{
    m_delegate.setAuthType(value);
}

void AsyncHttpClient::setExpectOnlyMessageBodyWithoutHeaders(bool expectOnlyBody)
{
    m_delegate.setExpectOnlyMessageBodyWithoutHeaders(expectOnlyBody);
}

AuthInfoCache::AuthorizationCacheItem AsyncHttpClient::authCacheItem() const
{
    return m_delegate.authCacheItem();
}

void AsyncHttpClient::forceEndOfMsgBody()
{
    m_delegate.forceEndOfMsgBody();
}

AsyncHttpClientPtr AsyncHttpClient::create()
{
    return AsyncHttpClientPtr(std::shared_ptr<AsyncHttpClient>(new AsyncHttpClient()));
}

AsyncHttpClientPtr AsyncHttpClient::create(std::unique_ptr<AbstractStreamSocket> socket)
{
    return AsyncHttpClientPtr(std::shared_ptr<AsyncHttpClient>(
        new AsyncHttpClient(std::move(socket))));
}

QString AsyncHttpClient::endpointWithProtocol(const nx::utils::Url& url)
{
    return lm("%1://%2:%3")
        .arg(url.scheme())
        .arg(url.host())
        .arg(url.port(nx::network::http::defaultPortForScheme(url.scheme().toLatin1())));
}

void AsyncHttpClient::stopWhileInAioThread()
{
    m_delegate.pleaseStopSync();
}

void AsyncHttpClient::onRequestHasBeenSent(bool isRetryAfterUnauthorizedResponse)
{
    std::shared_ptr<AsyncHttpClient> sharedThis(shared_from_this());
    emit requestHasBeenSent(sharedThis, isRetryAfterUnauthorizedResponse);
}

void AsyncHttpClient::onResponseReceived()
{
    std::shared_ptr<AsyncHttpClient> sharedThis(shared_from_this());
    emit responseReceived(sharedThis);
}

void AsyncHttpClient::onSomeMessageBodyAvailable()
{
    std::shared_ptr<AsyncHttpClient> sharedThis(shared_from_this());
    emit someMessageBodyAvailable(sharedThis);
}

void AsyncHttpClient::onDone()
{
    std::shared_ptr<AsyncHttpClient> sharedThis(shared_from_this());
    emit done(sharedThis);

    if (m_onDoneHandler)
        nx::utils::swapAndCall(m_onDoneHandler, sharedThis);
}

const char* AsyncHttpClient::toString(State state)
{
    switch (state)
    {
    case State::sInit:
        return "init";
    case State::sWaitingConnectToHost:
        return "waitingConnectToHost";
    case State::sSendingRequest:
        return "sendingRequest";
    case State::sReceivingResponse:
        return "receivingResponse";
    case State::sResponseReceived:
        return "responseReceived";
    case State::sReadingMessageBody:
        return "readingMessageBody";
    case State::sFailed:
        return "failed";
    case State::sDone:
        return "done";
    default:
        return "unknown";
    }
}

//-------------------------------------------------------------------------------------------------
// Utilities.

void downloadFileAsyncEx(
    const nx::utils::Url& url,
    DownloadCompletionHandlerEx completionHandlerEx,
    nx::network::http::AsyncHttpClientPtr httpClientCaptured,
    nx::network::http::Method::ValueType method)
{
    auto requestCompletionFunc = [httpClientCaptured, completionHandlerEx]
        (nx::network::http::AsyncHttpClientPtr httpClient) mutable
    {
        httpClientCaptured->disconnect(nullptr, (const char*)nullptr);
        httpClientCaptured.reset();

        if (httpClient->failed())
        {
            completionHandlerEx(
                SystemError::connectionReset,
                nx::network::http::StatusCode::ok,
                nx::network::http::StringType(),
                nx::network::http::BufferType(),
                nx::network::http::HttpHeaders());
            return;
        }

        if (httpClient->response()->statusLine.statusCode != nx::network::http::StatusCode::ok &&
            httpClient->response()->statusLine.statusCode != nx::network::http::StatusCode::partialContent)
        {
            completionHandlerEx(
                SystemError::noError,
                httpClient->response()->statusLine.statusCode,
                nx::network::http::StringType(),
                nx::network::http::BufferType(),
                nx::network::http::HttpHeaders());
            return;
        }

        completionHandlerEx(
            SystemError::noError,
            httpClient->response()->statusLine.statusCode,
            httpClient->contentType(),
            httpClient->fetchMessageBodyBuffer(),
            httpClient->response()->headers);
    };
    QObject::connect(
        httpClientCaptured.get(), &nx::network::http::AsyncHttpClient::done,
        httpClientCaptured.get(), requestCompletionFunc,
        Qt::DirectConnection);

    if (method == nx::network::http::Method::get)
    {
        httpClientCaptured->doGet(url);
    }
    else if (method == nx::network::http::Method::delete_)
    {
        httpClientCaptured->doDelete(url);
    }
    //else if (method == nx::network::http::Method::options)
    // {
    //    httpClientCaptured->doOptions(url);
    // }
    else
    {
        NX_ASSERT(0, "Unsupported HTTP method");
        requestCompletionFunc(httpClientCaptured);
    }
}

void downloadFileAsyncEx(
    const nx::utils::Url& url,
    DownloadCompletionHandlerEx completionHandlerEx,
    const nx::network::http::HttpHeaders& extraHeaders,
    AuthType authType,
    AsyncHttpClient::Timeouts timeouts,
    nx::network::http::Method::ValueType method)
{
    nx::network::http::AsyncHttpClientPtr httpClient = nx::network::http::AsyncHttpClient::create();
    httpClient->setAdditionalHeaders(extraHeaders);
    httpClient->setAuthType(authType);
    httpClient->setSendTimeoutMs(timeouts.sendTimeout.count());
    httpClient->setResponseReadTimeoutMs(timeouts.responseReadTimeout.count());
    httpClient->setMessageBodyReadTimeoutMs(timeouts.messageBodyReadTimeout.count());
    downloadFileAsyncEx(url, completionHandlerEx, std::move(httpClient), method);
}

void downloadFileAsync(
    const nx::utils::Url& url,
    DownloadCompletionHandler completionHandler,
    const nx::network::http::HttpHeaders& extraHeaders,
    AuthType authType,
    AsyncHttpClient::Timeouts timeouts)
{
    DownloadCompletionHandlerEx handler = [completionHandler](
        SystemError::ErrorCode osErrorCode,
        int statusCode,
        nx::network::http::StringType,
        nx::network::http::BufferType msgBody,
        nx::network::http::HttpHeaders httpHeaders)
        {
            completionHandler(osErrorCode, statusCode, msgBody, httpHeaders);
        };
    downloadFileAsyncEx(url, handler, extraHeaders, authType, timeouts);
}

SystemError::ErrorCode downloadFileSync(
    const nx::utils::Url& url,
    int* const statusCode,
    nx::network::http::BufferType* const msgBody,
    AsyncHttpClient::Timeouts timeouts)
{
    bool done = false;
    SystemError::ErrorCode resultingErrorCode = SystemError::noError;
    std::mutex mtx;
    std::condition_variable condVar;
    downloadFileAsync(
        url,
        [&resultingErrorCode, statusCode, msgBody, &mtx, &condVar, &done]
        (SystemError::ErrorCode errorCode, int _statusCode, const nx::network::http::BufferType& _msgBody,
            nx::network::http::HttpHeaders /*httpHeaders*/)
        {
            resultingErrorCode = errorCode;
            *statusCode = _statusCode;
            *msgBody = _msgBody;
            std::unique_lock<std::mutex> lk(mtx);
            done = true;
            condVar.notify_all();
        },
        nx::network::http::HttpHeaders(), AuthType::authBasicAndDigest, timeouts);

    std::unique_lock<std::mutex> lk(mtx);
    while (!done)
        condVar.wait(lk);

    return resultingErrorCode;
}

void uploadDataAsync(
    const nx::utils::Url& url,
    const QByteArray& data,
    const QByteArray& contentType,
    const nx::network::http::HttpHeaders& extraHeaders,
    const UploadCompletionHandler& callback,
    const AuthType authType,
    const QString& user,
    const QString& password,
    nx::network::http::Method::ValueType method)
{
    nx::network::http::AsyncHttpClientPtr httpClientHolder = nx::network::http::AsyncHttpClient::create();
    httpClientHolder->setAdditionalHeaders(extraHeaders);
    if (!user.isEmpty())
        httpClientHolder->setUserName(user);
    if (!password.isEmpty())
        httpClientHolder->setUserPassword(password);

    httpClientHolder->setAuthType(authType);

    auto completionFunc = [callback, httpClientHolder]
        (nx::network::http::AsyncHttpClientPtr httpClient) mutable
    {
        httpClientHolder->disconnect(nullptr, static_cast<const char *>(nullptr));
        httpClientHolder.reset();

        if (httpClient->failed())
            return callback(SystemError::connectionReset, nx::network::http::StatusCode::ok);

        const auto response = httpClient->response();
        const auto statusLine = response->statusLine;
        if ((statusLine.statusCode != nx::network::http::StatusCode::ok)
            && (statusLine.statusCode != nx::network::http::StatusCode::partialContent))
        {
            return callback(SystemError::noError, statusLine.statusCode);
        }

        callback(SystemError::noError, statusLine.statusCode);
    };

    QObject::connect(httpClientHolder.get(), &nx::network::http::AsyncHttpClient::done,
        httpClientHolder.get(), completionFunc, Qt::DirectConnection);

    if (method == nx::network::http::Method::put)
        httpClientHolder->doPut(url, contentType, data);
    else if (method.isEmpty() || method == nx::network::http::Method::post)
        httpClientHolder->doPost(url, contentType, data);
    else
        NX_ASSERT(0, "Unsupported HTTP method");
}

SystemError::ErrorCode uploadDataSync(
    const nx::utils::Url& url,
    const QByteArray& data,
    const QByteArray& contentType,
    const QString& user,
    const QString& password,
    const AuthType authType,
    nx::network::http::StatusCode::Value* httpCode)
{
    bool done = false;
    SystemError::ErrorCode result = SystemError::noError;
    std::mutex mutex;
    std::condition_variable waiter;


    const auto callback = [&result, &mutex, &waiter, &done, httpCode]
        (SystemError::ErrorCode errorCode, int statusCode)
    {
        result = errorCode;
        if (httpCode)
        {
            *httpCode = (errorCode == SystemError::noError
                ? static_cast<nx::network::http::StatusCode::Value>(statusCode)
                : nx::network::http::StatusCode::internalServerError);
        }

        std::unique_lock<std::mutex> lk(mutex);
        done = true;
        waiter.notify_all();
    };

    uploadDataAsync(url, data, contentType
        , nx::network::http::HttpHeaders(), callback, authType, user, password);

    std::unique_lock<std::mutex> guard(mutex);
    while (!done)
        waiter.wait(guard);

    return result;
}

} // namespace http
} // namespace network
} // namespace nx
