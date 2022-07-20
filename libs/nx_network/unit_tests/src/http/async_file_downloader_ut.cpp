// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <QtCore/QTemporaryFile>

#include <nx/network/http/async_file_downloader.h>
#include <nx/network/http/empty_message_body_source.h>
#include <nx/network/http/server/proxy/proxy_handler.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/http/writable_message_body.h>
#include <nx/network/test_support/message_body.h>
#include <nx/network/test_support/synchronous_tcp_server.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/thread/barrier_handler.h>

#include "repeating_buffer_msg_body_source.h"

namespace nx::network::http::test {

static constexpr char kFilePath[] = "/file.zip";
static constexpr char kInfiniteFilePath[] = "/infiniteFile.zip";
static constexpr char kPartialFilePath[] = "/partialFile.zip";
static constexpr char kEmptyFilePath[] = "/emptyFile.zip";
static constexpr int kContentLength = 10 * 1024 * 1024;
static const nx::Buffer kLine10Chars = "123456789\n";
static constexpr auto k50msTimeout = std::chrono::milliseconds(50);

class TransparentProxyHandler:
    public server::proxy::AbstractProxyHandler
{
public:
    TransparentProxyHandler(const SocketAddress& target):
        m_target(target)
    {
    }

protected:
    virtual void detectProxyTarget(
        const ConnectionAttrs&,
        const SocketAddress&,
        Request* const request,
        ProxyTargetDetectedHandler handler)
    {
        // Replacing the Host header with the proxy target as specified in [rfc7230; 5.4].
        insertOrReplaceHeader(&request->headers, {"Host", m_target.toString()});
        handler(StatusCode::ok, m_target);
    }

private:
    const SocketAddress m_target;
};

class AsyncFileDownloader:
    public ::testing::Test
{
public:
    AsyncFileDownloader()
    {
    }

    ~AsyncFileDownloader()
    {
    }

protected:
    static std::unique_ptr<TestHttpServer> m_httpServer;
    static std::unique_ptr<TestHttpServer> m_proxy;
    static std::unique_ptr<TestHttpServer> m_transparentProxy;
    static Credentials m_credentials;
    static Credentials m_proxyCredentials;

    virtual void SetUp() override
    {
        if (!m_httpServer)
            SetUpTestSuite();
    }

    static void SetUpTestSuite()
    {
        m_httpServer = std::make_unique<TestHttpServer>();
        m_proxy = std::make_unique<TestHttpServer>(server::Role::proxy);
        m_transparentProxy = std::make_unique<TestHttpServer>(server::Role::proxy);

        m_credentials = Credentials("username", PasswordAuthToken("password"));
        m_proxyCredentials = Credentials("proxy", PasswordAuthToken("XpasswordX"));

        m_httpServer->registerRequestProcessorFunc(
            kFilePath,
            [](auto&&... args)
            {
                serve(
                    std::bind(&AsyncFileDownloader::sendFileResponse,
                        std::placeholders::_1, std::placeholders::_2),
                    std::forward<decltype(args)>(args)...);
            });
        m_httpServer->registerRequestProcessorFunc(
            kInfiniteFilePath,
            [](auto&&... args)
            {
                serve(
                    std::bind(&AsyncFileDownloader::sendInfiniteFileResponse,
                        std::placeholders::_1, std::placeholders::_2),
                    std::forward<decltype(args)>(args)...);
            });
        m_httpServer->registerRequestProcessorFunc(
            kPartialFilePath,
            [](auto&&... args)
            {
                serve(
                    std::bind(&AsyncFileDownloader::sendPartialFileResponse,
                        std::placeholders::_1, std::placeholders::_2),
                    std::forward<decltype(args)>(args)...);
            });
        m_httpServer->registerRequestProcessorFunc(
            kEmptyFilePath,
            [](auto&&... args)
            {
                serve(
                    std::bind(&AsyncFileDownloader::sendEmptyFileResponse,
                        std::placeholders::_1, std::placeholders::_2),
                    std::forward<decltype(args)>(args)...);
            });

        m_httpServer->enableAuthentication(".*");
        m_httpServer->registerUserCredentials(m_credentials);
        ASSERT_TRUE(m_httpServer->bindAndListen());

        m_proxy->registerRequestProcessor<server::proxy::ProxyHandler>(kAnyPath);
        m_proxy->enableAuthentication(".*");
        m_proxy->registerUserCredentials(m_proxyCredentials);
        ASSERT_TRUE(m_proxy->bindAndListen());

        m_transparentProxy->registerRequestProcessor(
            kAnyPath,
            []()
            {
                return std::make_unique<TransparentProxyHandler>(
                    m_httpServer->serverAddress());
            });
        m_transparentProxy->enableAuthentication(".*");
        m_transparentProxy->registerUserCredentials(m_proxyCredentials);
        ASSERT_TRUE(m_transparentProxy->bindAndListen());
    }

    static void TearDownTestSuite()
    {
        m_proxy.reset();
        m_transparentProxy.reset();
        m_httpServer.reset();
    }

    static void sendFileResponse(
        nx::network::http::RequestContext /*requestContext*/,
        nx::network::http::RequestProcessedHandler completionHandler)
    {
        http::RequestResult result(StatusCode::ok);
        auto body = std::make_unique<WritableMessageBody>(
            "application/octet-stream",
            kContentLength);
        auto writer = body->writer();
        result.body = std::move(body);
        completionHandler(std::move(result));

        for (int i = 0; i < kContentLength/10 ; i++)
            writer->writeBodyData(kLine10Chars);
        writer->writeEof();
    }

    static void sendInfiniteFileResponse(
        nx::network::http::RequestContext /*requestContext*/,
        nx::network::http::RequestProcessedHandler completionHandler)
    {
        http::RequestResult result(StatusCode::ok);
        result.body = std::make_unique<RepeatingBufferMsgBodySource>(
            "application/octet-stream",
            kLine10Chars);
        completionHandler(std::move(result));
    }

    static void sendPartialFileResponse(
        nx::network::http::RequestContext /*requestContext*/,
        nx::network::http::RequestProcessedHandler completionHandler)
    {
        http::RequestResult result(StatusCode::ok);
        auto body = std::make_unique<WritableMessageBody>(
            "application/octet-stream",
            kContentLength);
        auto writer = body->writer();
        writer->writeBodyData(kLine10Chars);
        result.body = std::move(body);
        completionHandler(std::move(result));
    }

    static void sendEmptyFileResponse(
        nx::network::http::RequestContext /*requestContext*/,
        nx::network::http::RequestProcessedHandler completionHandler)
    {
        http::RequestResult result(StatusCode::ok);
        result.body = std::make_unique<EmptyMessageBodySource>(
            "application/octet-stream",
            kContentLength);
        completionHandler(std::move(result));
    }

    template<typename SendFunc>
    static void serve(
        SendFunc send,
        nx::network::http::RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler)
    {
        const auto authHeaderIter =
            requestContext.request.headers.find(header::Authorization::NAME);
        if (authHeaderIter != requestContext.request.headers.end())
        {
            if (header::DigestAuthorization authorizationHeader;
                authorizationHeader.parse(authHeaderIter->second) &&
                authorizationHeader.authScheme == header::AuthScheme::digest)
            {
                if (validateAuthorization(
                        requestContext.request.requestLine.method,
                        m_credentials,
                        authorizationHeader))
                {
                    send(std::move(requestContext), std::move(completionHandler));
                    return;
                }
            }
        }

        header::WWWAuthenticate wwwAuthenticate;
        wwwAuthenticate.authScheme = header::AuthScheme::digest;
        wwwAuthenticate.params.emplace("nonce", "not_random_nonce");
        wwwAuthenticate.params.emplace("realm", "realm_of_possibility");
        wwwAuthenticate.params.emplace("algorithm", "MD5");

        RequestResult result(StatusCode::unauthorized);

        result.headers.emplace(
            header::WWWAuthenticate::NAME,
            wwwAuthenticate.serialized());

        result.headers.emplace("Connection", "close");

        result.body = std::make_unique<CustomLengthBody>(
            "text/plain",
            "HTTP/1.1 401 Unauthorized\r\nContent-Length: 0\r\n\r\n",
            0);

        completionHandler(std::move(result));
    }

    nx::utils::Url prepareUrl(const std::string& requestPath = std::string())
    {
        const auto serverAddress = m_httpServer->serverAddress();
        return nx::network::url::Builder().setScheme(nx::network::http::kSecureUrlSchemeName)
            .setEndpoint(serverAddress).setPath(requestPath);
    }

    struct Context
    {
        std::unique_ptr<http::AsyncFileDownloader> downloader;
        std::shared_ptr<QTemporaryFile> file;
        std::future<std::optional<size_t>> responseFuture;
        std::future<bool> doneFuture;
        size_t contentLength = 0;
        size_t downloaded = 0;
        int calls = 0;
    };

    std::unique_ptr<Context> prepareTestContext(
        ssl::AdapterFunc adapterFunc,
        const AsyncClient::Timeouts& timeouts)
    {
        auto responsePromise = std::make_unique<std::promise<std::optional<size_t>>>();
        auto donePromise = std::make_unique<std::promise<bool>>();

        auto context = std::make_unique<Context>(Context{
            std::make_unique<http::AsyncFileDownloader>(std::move(adapterFunc)),
            std::make_shared<QTemporaryFile>(),
            responsePromise->get_future(),
            donePromise->get_future()});
        auto contextPtr = context.get();

        context->downloader->setTimeouts(timeouts);
        context->downloader->setCredentials(m_credentials);
        context->downloader->setOnResponseReceived(
            [contextPtr, responsePromise = std::move(responsePromise)](auto contentLength)
            {
                responsePromise->set_value(contentLength);
                contextPtr->contentLength = contentLength.value_or(0);
            });
        context->downloader->setOnProgressHasBeenMade(
            [contextPtr](auto&& buf, auto percentage)
            {
                contextPtr->downloaded += buf.size();
                if (percentage)
                {
                    ASSERT_EQ(*percentage, (double)contextPtr->downloaded / contextPtr->contentLength);
                }
                contextPtr->calls++;
            });
        context->downloader->setOnDone(
            [contextPtr, donePromise = std::move(donePromise)]()
            {
                donePromise->set_value(!contextPtr->downloader->failed());
            });

        context->file->open();

        return context;
    }

    bool verifyFileContent(std::shared_ptr<QTemporaryFile> file)
    {
        file->seek(0);
        for (int i = 0; i < file->size() ; i += 10)
        {
            if(file->readLine() != kLine10Chars)
                return false;
        }
        return true;
    }
};

std::unique_ptr<TestHttpServer> AsyncFileDownloader::m_httpServer;
std::unique_ptr<TestHttpServer> AsyncFileDownloader::m_proxy;
std::unique_ptr<TestHttpServer> AsyncFileDownloader::m_transparentProxy;
Credentials AsyncFileDownloader::m_credentials;
Credentials AsyncFileDownloader::m_proxyCredentials;

TEST_F(AsyncFileDownloader, successfull)
{
    auto context = prepareTestContext(
        ssl::kAcceptAnyCertificate,
        {kNoTimeout, kNoTimeout, kNoTimeout});
    context->downloader->start(prepareUrl(kFilePath), context->file);

    auto contentLength = context->responseFuture.get().value();
    ASSERT_EQ(contentLength, kContentLength);

    ASSERT_TRUE(context->doneFuture.get());

    ASSERT_TRUE(context->downloader->hasRequestSucceeded());
    ASSERT_GT(context->calls, 0);
    ASSERT_EQ(context->downloaded, contentLength);
    ASSERT_EQ(context->downloaded, context->file->size());
    ASSERT_TRUE(verifyFileContent(context->file));
}

TEST_F(AsyncFileDownloader, interrupted)
{
    auto context = prepareTestContext(
        ssl::kAcceptAnyCertificate,
        {kNoTimeout, kNoTimeout, kNoTimeout});
    context->downloader->start(prepareUrl(kInfiniteFilePath), context->file);

    ASSERT_EQ(context->responseFuture.get(), std::nullopt);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    context->downloader->pleaseStopSync();

    auto status = context->doneFuture.wait_for(std::chrono::milliseconds(100));
    ASSERT_EQ(status, std::future_status::timeout);

    ASSERT_TRUE(context->downloader->hasRequestSucceeded());
    ASSERT_EQ(SystemError::noError, context->downloader->lastSysErrorCode());
    ASSERT_GT(context->calls, 0);
    ASSERT_GT(context->downloaded, 0);
    ASSERT_EQ(context->downloaded, context->file->size());
}

TEST_F(AsyncFileDownloader, interruptedBeforeContent)
{
    auto context = prepareTestContext(
        ssl::kAcceptAnyCertificate,
        {kNoTimeout, kNoTimeout, kNoTimeout});
    context->downloader->start(prepareUrl(kInfiniteFilePath), context->file);

    ASSERT_EQ(context->responseFuture.get(), std::nullopt);
    context->downloader->pleaseStopSync();

    auto status = context->doneFuture.wait_for(std::chrono::milliseconds(100));
    ASSERT_EQ(status, std::future_status::timeout);

    ASSERT_TRUE(context->downloader->hasRequestSucceeded());
    ASSERT_EQ(SystemError::noError, context->downloader->lastSysErrorCode());
    ASSERT_EQ(context->calls, 0);
    ASSERT_EQ(context->downloaded, 0);
    ASSERT_EQ(context->downloaded, context->file->size());
}

TEST_F(AsyncFileDownloader, resetConnection)
{
    auto context = prepareTestContext(
        ssl::kAcceptAnyCertificate,
        {kNoTimeout, kNoTimeout, kNoTimeout});
    context->downloader->start(prepareUrl(kPartialFilePath), context->file);

    auto contentLength = context->responseFuture.get().value();
    ASSERT_EQ(contentLength, kContentLength);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    m_httpServer->server().closeAllConnections();
    ASSERT_FALSE(context->doneFuture.get());

    ASSERT_FALSE(context->downloader->hasRequestSucceeded());
    ASSERT_EQ(SystemError::connectionReset, context->downloader->lastSysErrorCode());
    ASSERT_GT(context->calls, 0);
    ASSERT_LT(context->downloaded, contentLength);
    ASSERT_EQ(context->downloaded, context->file->size());
    ASSERT_TRUE(verifyFileContent(context->file));
}

TEST_F(AsyncFileDownloader, refusedConnection)
{
    auto context = prepareTestContext(
        ssl::kAcceptAnyCertificate,
        {kNoTimeout, kNoTimeout, kNoTimeout});
    auto url = prepareUrl(kFilePath);
    m_httpServer.reset();
    context->downloader->start(url, context->file);

    auto status = context->responseFuture.wait_for(std::chrono::milliseconds(100));
    ASSERT_EQ(status, std::future_status::timeout);
    ASSERT_FALSE(context->doneFuture.get());

    ASSERT_FALSE(context->downloader->hasRequestSucceeded());
    ASSERT_EQ(SystemError::connectionRefused, context->downloader->lastSysErrorCode());
    ASSERT_EQ(context->calls, 0);
    ASSERT_EQ(context->downloaded, 0);
    ASSERT_EQ(context->downloaded, context->file->size());
}

TEST_F(AsyncFileDownloader, timedOut)
{
    auto context = prepareTestContext(
        ssl::kAcceptAnyCertificate,
        {kNoTimeout, kNoTimeout, k50msTimeout});
    context->downloader->start(prepareUrl(kPartialFilePath), context->file);

    auto contentLength = context->responseFuture.get().value();
    ASSERT_EQ(contentLength, kContentLength);

    ASSERT_FALSE(context->doneFuture.get());

    ASSERT_FALSE(context->downloader->hasRequestSucceeded());
    ASSERT_EQ(SystemError::timedOut, context->downloader->lastSysErrorCode());
    ASSERT_GT(context->calls, 0);
    ASSERT_LT(context->downloaded, contentLength);
    ASSERT_EQ(context->downloaded, context->file->size());
}

TEST_F(AsyncFileDownloader, timedOutBeforeContent)
{
    auto context = prepareTestContext(
        ssl::kAcceptAnyCertificate,
        {kNoTimeout, kNoTimeout, k50msTimeout});
    context->downloader->start(prepareUrl(kEmptyFilePath), context->file);

    auto contentLength = context->responseFuture.get().value();
    ASSERT_EQ(contentLength, kContentLength);

    ASSERT_FALSE(context->doneFuture.get());

    ASSERT_FALSE(context->downloader->hasRequestSucceeded());
    ASSERT_EQ(SystemError::timedOut, context->downloader->lastSysErrorCode());
    ASSERT_EQ(context->calls, 0);
    ASSERT_EQ(context->downloaded, 0);
    ASSERT_EQ(context->downloaded, context->file->size());
}

TEST_F(AsyncFileDownloader, sequential)
{
    auto context = prepareTestContext(
        ssl::kAcceptAnyCertificate,
        {kNoTimeout, kNoTimeout, kNoTimeout});
    context->downloader->start(prepareUrl(kFilePath), context->file);

    auto contentLength = context->responseFuture.get().value();
    ASSERT_EQ(contentLength, kContentLength);

    ASSERT_TRUE(context->doneFuture.get());

    ASSERT_TRUE(context->downloader->hasRequestSucceeded());
    ASSERT_GT(context->calls, 0);
    ASSERT_EQ(context->downloaded, contentLength);
    ASSERT_EQ(context->downloaded, context->file->size());
    ASSERT_TRUE(verifyFileContent(context->file));

    context->downloader->pleaseStopSync();

    context->file = std::make_shared<QTemporaryFile>();
    ASSERT_TRUE(context->file->open());

    contentLength = 0;
    context->downloaded = 0;
    context->calls = 0;
    auto secondPromise = std::make_unique<std::promise<bool>>();
    auto secondFuture = secondPromise->get_future();
    context->downloader->setOnResponseReceived(
        [&](auto v)
        {
            ASSERT_FALSE(context->downloader->failed());
            contentLength = *v;
            ASSERT_EQ(contentLength, kContentLength);
        });
    context->downloader->setOnDone(
        [&, secondPromise = std::move(secondPromise)]()
        {
            secondPromise->set_value(!context->downloader->failed());
        });
    context->downloader->start(prepareUrl(kFilePath), context->file);

    ASSERT_TRUE(secondFuture.get());

    ASSERT_TRUE(context->downloader->hasRequestSucceeded());
    ASSERT_GT(context->calls, 0);
    ASSERT_EQ(context->downloaded, contentLength);
    ASSERT_EQ(context->downloaded, context->file->size());
    ASSERT_TRUE(verifyFileContent(context->file));
}

TEST_F(AsyncFileDownloader, viaProxy)
{
    auto context = prepareTestContext(
        ssl::kAcceptAnyCertificate,
        {kNoTimeout, kNoTimeout, kNoTimeout});
    context->downloader->setProxyCredentials(m_proxyCredentials);
    context->downloader->setProxyVia(m_proxy->serverAddress(), true, ssl::kAcceptAnyCertificate);
    context->downloader->start(prepareUrl(kFilePath), context->file);

    auto contentLength = context->responseFuture.get().value();
    ASSERT_EQ(contentLength, kContentLength);

    ASSERT_TRUE(context->doneFuture.get());

    ASSERT_TRUE(context->downloader->hasRequestSucceeded());
    ASSERT_GT(context->calls, 0);
    ASSERT_EQ(context->downloaded, contentLength);
    ASSERT_EQ(context->downloaded, context->file->size());
    ASSERT_TRUE(verifyFileContent(context->file));
}

TEST_F(AsyncFileDownloader, viaTransparentProxy)
{
    auto context = prepareTestContext(
        ssl::kAcceptAnyCertificate,
        {kNoTimeout, kNoTimeout, kNoTimeout});
    context->downloader->setCredentials(m_credentials);
    context->downloader->setProxyCredentials(m_proxyCredentials);
    auto url = nx::network::url::Builder().setScheme(nx::network::http::kSecureUrlSchemeName)
        .setEndpoint(m_transparentProxy->serverAddress()).setPath(kFilePath);
    context->downloader->start(url, context->file);

    auto contentLength = context->responseFuture.get().value();
    ASSERT_EQ(contentLength, kContentLength);

    ASSERT_TRUE(context->doneFuture.get());

    ASSERT_TRUE(context->downloader->hasRequestSucceeded());
    ASSERT_GT(context->calls, 0);
    ASSERT_EQ(context->downloaded, contentLength);
    ASSERT_EQ(context->downloaded, context->file->size());
    ASSERT_TRUE(verifyFileContent(context->file));
}

} // namespace nx::network::http::test
