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

#include "auth_tools.h"
#include "buffer_source.h"
#include "custom_headers.h"

static const int DEFAULT_SEND_TIMEOUT = 3000;
static const int DEFAULT_RESPONSE_READ_TIMEOUT = 3000;

using std::make_pair;

namespace nx_http {

static constexpr size_t RESPONSE_BUFFER_SIZE = 16 * 1024;
static constexpr int kMaxNumberOfRedirects = 5;

AsyncHttpClient::AsyncHttpClient()
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
    pleaseStopSync(false);
}

void AsyncHttpClient::pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    m_delegate.pleaseStop(std::move(completionHandler));
}

void AsyncHttpClient::pleaseStopSync(bool checkForLocks)
{
    m_delegate.pleaseStopSync(checkForLocks);
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
    const nx_http::StringType& contentType,
    nx_http::StringType messageBodyBuffer,
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
    const nx_http::StringType& contentType,
    nx_http::StringType messageBody,
    bool includeContentLength,
    nx::utils::MoveOnlyFunc<void(AsyncHttpClientPtr)> completionHandler)
{
    m_onDoneHandler = std::move(completionHandler);
    doPost(url, contentType, std::move(messageBody), includeContentLength);
}

void AsyncHttpClient::doPut(
    const nx::utils::Url& url,
    const nx_http::StringType& contentType,
    nx_http::StringType messageBodyBuffer)
{
    auto messageBody = std::make_unique<BufferSource>(
        contentType,
        std::move(messageBodyBuffer));
    m_delegate.setRequestBody(std::move(messageBody));
    m_delegate.doPut(url);
}

void AsyncHttpClient::doPut(
    const nx::utils::Url& url,
    const nx_http::StringType& contentType,
    nx_http::StringType messageBody,
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

const nx_http::Request& AsyncHttpClient::request() const
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

void AsyncHttpClient::setProxyVia(const SocketAddress& proxyEndpoint)
{
    m_delegate.setProxyVia(proxyEndpoint);
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

QString AsyncHttpClient::endpointWithProtocol(const nx::utils::Url& url)
{
    return lm("%1://%2:%3")
        .arg(url.scheme())
        .arg(url.host())
        .arg(url.port(nx_http::defaultPortForScheme(url.scheme().toLatin1())));
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
    std::function<void(SystemError::ErrorCode, int, nx_http::StringType, nx_http::BufferType)> completionHandler,
    nx_http::AsyncHttpClientPtr httpClientCaptured)
{
    auto requestCompletionFunc = [httpClientCaptured, completionHandler]
        (nx_http::AsyncHttpClientPtr httpClient) mutable
    {
        httpClientCaptured->disconnect(nullptr, (const char*)nullptr);
        httpClientCaptured.reset();

        if (httpClient->failed())
        {
            completionHandler(
                SystemError::connectionReset,
                nx_http::StatusCode::ok,
                nx_http::StringType(),
                nx_http::BufferType());
            return;
        }

        if (httpClient->response()->statusLine.statusCode != nx_http::StatusCode::ok &&
            httpClient->response()->statusLine.statusCode != nx_http::StatusCode::partialContent)
        {
            completionHandler(
                SystemError::noError,
                httpClient->response()->statusLine.statusCode,
                nx_http::StringType(),
                nx_http::BufferType());
            return;
        }

        completionHandler(
            SystemError::noError,
            httpClient->response()->statusLine.statusCode,
            httpClient->contentType(),
            httpClient->fetchMessageBodyBuffer());
    };
    QObject::connect(
        httpClientCaptured.get(), &nx_http::AsyncHttpClient::done,
        httpClientCaptured.get(), requestCompletionFunc,
        Qt::DirectConnection);

    httpClientCaptured->doGet(url);
}

void downloadFileAsync(
    const nx::utils::Url& url,
    std::function<void(SystemError::ErrorCode, int, nx_http::BufferType)> completionHandler,
    const nx_http::HttpHeaders& extraHeaders,
    AuthType authType,
    AsyncHttpClient::Timeouts timeouts)
{
    auto handler = [completionHandler](
        SystemError::ErrorCode osErrorCode,
        int statusCode,
        nx_http::StringType,
        nx_http::BufferType msgBody)
    {
        completionHandler(osErrorCode, statusCode, msgBody);
    };
    downloadFileAsyncEx(url, handler, extraHeaders, authType, timeouts);
}

void downloadFileAsyncEx(
    const nx::utils::Url& url,
    std::function<void(SystemError::ErrorCode, int, nx_http::StringType, nx_http::BufferType)> completionHandler,
    const nx_http::HttpHeaders& extraHeaders,
    AuthType authType,
    AsyncHttpClient::Timeouts timeouts)
{
    nx_http::AsyncHttpClientPtr httpClient = nx_http::AsyncHttpClient::create();
    httpClient->setAdditionalHeaders(extraHeaders);
    httpClient->setAuthType(authType);
    httpClient->setSendTimeoutMs(timeouts.sendTimeout.count());
    httpClient->setResponseReadTimeoutMs(timeouts.responseReadTimeout.count());
    httpClient->setMessageBodyReadTimeoutMs(timeouts.messageBodyReadTimeout.count());
    downloadFileAsyncEx(url, completionHandler, std::move(httpClient));
}

SystemError::ErrorCode downloadFileSync(
    const nx::utils::Url& url,
    int* const statusCode,
    nx_http::BufferType* const msgBody)
{
    bool done = false;
    SystemError::ErrorCode resultingErrorCode = SystemError::noError;
    std::mutex mtx;
    std::condition_variable condVar;
    downloadFileAsync(
        url,
        [&resultingErrorCode, statusCode, msgBody, &mtx, &condVar, &done]
    (SystemError::ErrorCode errorCode, int _statusCode, const nx_http::BufferType& _msgBody) {
        resultingErrorCode = errorCode;
        *statusCode = _statusCode;
        *msgBody = _msgBody;
        std::unique_lock<std::mutex> lk(mtx);
        done = true;
        condVar.notify_all();
    });

    std::unique_lock<std::mutex> lk(mtx);
    while (!done)
        condVar.wait(lk);

    return resultingErrorCode;
}

void uploadDataAsync(
    const nx::utils::Url& url,
    const QByteArray& data,
    const QByteArray& contentType,
    const nx_http::HttpHeaders& extraHeaders,
    const UploadCompletionHandler& callback,
    const AuthType authType,
    const QString& user,
    const QString& password)
{
    nx_http::AsyncHttpClientPtr httpClientHolder = nx_http::AsyncHttpClient::create();
    httpClientHolder->setAdditionalHeaders(extraHeaders);
    if (!user.isEmpty())
        httpClientHolder->setUserName(user);
    if (!password.isEmpty())
        httpClientHolder->setUserPassword(password);

    httpClientHolder->setAuthType(authType);

    auto completionFunc = [callback, httpClientHolder]
        (nx_http::AsyncHttpClientPtr httpClient) mutable
    {
        httpClientHolder->disconnect(nullptr, static_cast<const char *>(nullptr));
        httpClientHolder.reset();

        if (httpClient->failed())
            return callback(SystemError::connectionReset, nx_http::StatusCode::ok);

        const auto response = httpClient->response();
        const auto statusLine = response->statusLine;
        if ((statusLine.statusCode != nx_http::StatusCode::ok)
            && (statusLine.statusCode != nx_http::StatusCode::partialContent))
        {
            return callback(SystemError::noError, statusLine.statusCode);
        }

        callback(SystemError::noError, statusLine.statusCode);
    };

    QObject::connect(httpClientHolder.get(), &nx_http::AsyncHttpClient::done,
        httpClientHolder.get(), completionFunc, Qt::DirectConnection);

    httpClientHolder->doPost(url, contentType, data);
}

SystemError::ErrorCode uploadDataSync(
    const nx::utils::Url& url,
    const QByteArray& data,
    const QByteArray& contentType,
    const QString& user,
    const QString& password,
    const AuthType authType,
    nx_http::StatusCode::Value* httpCode)
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
                ? static_cast<nx_http::StatusCode::Value>(statusCode)
                : nx_http::StatusCode::internalServerError);
        }

        std::unique_lock<std::mutex> lk(mutex);
        done = true;
        waiter.notify_all();
    };

    uploadDataAsync(url, data, contentType
        , nx_http::HttpHeaders(), callback, authType, user, password);

    std::unique_lock<std::mutex> guard(mutex);
    while (!done)
        waiter.wait(guard);

    return result;
}

} // namespace nx_http
