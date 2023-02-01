// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <fstream>

#include <gtest/gtest.h>

#include <nx/network/http/http_client.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/http/server/handler/file_handler.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/random.h>
#include <nx/utils/test_support/test_with_temporary_directory.h>

namespace nx::network::http::server::handler::test {

static constexpr char kResourceFilePath[] = ":/content/filename.txt";
static constexpr char kResourceFileHttpPath[] = "/content/filename.txt";

class HttpServerFileDownloader:
    public ::testing::Test,
    public nx::utils::test::TestWithTemporaryDirectory
{
public:
    struct EventHandlers
    {
        std::optional<nx::utils::MoveOnlyFunc<void(bool)>> onRequestHasBeenSent;
    };

    ~HttpServerFileDownloader()
    {
        m_timer.executeInAioThreadSync([this]()
        {
            m_timer.pleaseStopSync();
            m_httpAsyncClient.reset();
        });
    }

protected:
    virtual void SetUp() override
    {
        QFile resourceFile(kResourceFilePath);
        ASSERT_TRUE(resourceFile.open(QIODevice::ReadOnly));
        m_resourceFileContents = resourceFile.readAll();

        m_regularFileContents = nx::utils::generateRandomName(128*1024);
        m_filePath = testDataDir().toStdString() + "/test.txt";
        std::ofstream f(m_filePath.c_str(), std::ios_base::out | std::ios_base::binary);
        f << m_regularFileContents.toStdString();

        m_fileReadSize = std::max<std::size_t>(m_resourceFileContents.size() / 3, 1);

        ASSERT_TRUE(m_server.bindAndListen());

        m_server.httpMessageDispatcher().registerRequestProcessor(
            "/fs/.*",
            [this]() { return std::make_unique<FileDownloader>(
                "/fs/", testDataDir().toStdString(), m_fileReadSize); },
            Method::get);

        m_server.httpMessageDispatcher().registerRequestProcessor(
            "/content/.*",
            [this]() { return std::make_unique<FileDownloader>(
                "/content/", ":/content/", m_fileReadSize); },
            Method::get);

        m_httpClient = std::make_unique<HttpClient>(ssl::kAcceptAnyCertificate);
        m_httpClient->setTimeouts(AsyncClient::kInfiniteTimeouts);

        m_httpAsyncClient = std::make_unique<AsyncClient>(ssl::kAcceptAnyCertificate);
        m_httpAsyncClient->setTimeouts(AsyncClient::kInfiniteTimeouts);
        m_httpAsyncClient->bindToAioThread(m_timer.getAioThread());
    }

    void whenRequestResourceFile()
    {
        m_expectedFileContents = &m_resourceFileContents;
        whenRequestFile(kResourceFileHttpPath);
    }

    void whenRequestExistingFile()
    {
        m_expectedFileContents = &m_regularFileContents;
        whenRequestFile("/fs/test.txt");
    }

    void whenRequestExistingFileAsync(EventHandlers handlers = {})
    {
        m_expectedFileContents = &m_regularFileContents;
        whenRequestFileAsync("/fs/test.txt", std::move(handlers));
    }

    void whenRequestFile(const std::string& path)
    {
        ASSERT_TRUE(m_httpClient->doGet(buildUrl(path)));
    }

    void whenRequestFileAsync(const std::string& path, EventHandlers handlers)
    {
        if (handlers.onRequestHasBeenSent)
        {
            m_httpAsyncClient->setOnRequestHasBeenSent(
                [h = std::move(*handlers.onRequestHasBeenSent)](bool result)
                {
                    h(result);
                });
        }

        m_httpAsyncClient->doGet(buildUrl(path));
    }

    void whenCloseConnection()
    {
        m_httpClient.reset();
    }

    void thenRequestSucceeded()
    {
        thenRequestCompletedWith(StatusCode::ok);
    }

    void thenRequestCompletedWith(StatusCode::Value statusCode)
    {
        ASSERT_NE(nullptr, m_httpClient->response());
        ASSERT_EQ(statusCode, m_httpClient->response()->statusLine.statusCode);
    }

    void andContentLengthIsEqualToFileSize()
    {
        ASSERT_EQ(
            m_expectedFileContents->size(),
            nx::utils::stoi(getHeaderValue(m_httpClient->response()->headers, "Content-Length")));
    }

    void andFileDownloaded()
    {
        const auto responseBody = m_httpClient->fetchEntireMessageBody();
        ASSERT_TRUE(responseBody);
        ASSERT_EQ(*m_expectedFileContents, *responseBody);
    }

    aio::Timer& timer()
    {
        return m_timer;
    }

    void closeClientConnection()
    {
        m_httpAsyncClient.reset();
    }

private:
    std::string m_filePath;
    std::unique_ptr<HttpClient> m_httpClient;
    aio::Timer m_timer;
    std::unique_ptr<AsyncClient> m_httpAsyncClient;
    TestHttpServer m_server;
    nx::Buffer m_resourceFileContents;
    nx::Buffer m_regularFileContents;
    nx::Buffer* m_expectedFileContents = nullptr;
    std::size_t m_fileReadSize = 1;

    nx::utils::Url buildUrl(const std::string& path)
    {
        auto url = url::Builder().setScheme(kUrlSchemeName)
            .setEndpoint(m_server.serverAddress()).toUrl();
        // NOTE: Not using Builder::setPath intentionally so that path stays unnormalized.
        url.setPath(QString::fromStdString(path));
        return url;
    }
};

TEST_F(HttpServerFileDownloader, provides_regular_file)
{
    whenRequestExistingFile();

    thenRequestSucceeded();
    andContentLengthIsEqualToFileSize();
    andFileDownloaded();
}

TEST_F(HttpServerFileDownloader, provides_qt_resource_file)
{
    whenRequestResourceFile();

    thenRequestSucceeded();
    andContentLengthIsEqualToFileSize();
    andFileDownloaded();
}

TEST_F(HttpServerFileDownloader, reports_not_found_for_absent_file)
{
    whenRequestFile("/fs/there_is_no_such_file.txt");
    thenRequestCompletedWith(StatusCode::notFound);
}

TEST_F(HttpServerFileDownloader, access_to_parent_dir_is_forbidden)
{
    whenRequestFile("/fs/../file_from_parent_dir");
    thenRequestCompletedWith(StatusCode::forbidden);
}

TEST_F(HttpServerFileDownloader, file_io_is_cancelled_properly_when_connection_interrupted)
{
    std::promise<void> done;

    whenRequestExistingFileAsync({
        .onRequestHasBeenSent = [this, &done](bool /*result*/) {
            const auto delay = std::chrono::milliseconds(nx::utils::random::number<int>(0, 10));
            auto f = [this, &done]()
            {
                closeClientConnection();
                done.set_value();
            };

            if (delay == std::chrono::milliseconds::zero())
                f();
            else
                timer().start(delay, f);
        }
    });

    done.get_future().wait();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // then process does not crash.
}

} // namespace nx::network::http::server::handler::test
