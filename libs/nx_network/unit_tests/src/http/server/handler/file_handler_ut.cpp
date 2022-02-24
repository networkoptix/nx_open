// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <fstream>

#include <gtest/gtest.h>

#include <nx/network/http/http_client.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/http/server/handler/file_handler.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/test_support/test_with_temporary_directory.h>

namespace nx::network::http::server::handler::test {

static constexpr char kResourceFilePath[] = ":/content/filename.txt";
static constexpr char kResourceFileHttpPath[] = "/content/filename.txt";

class HttpServerFileDownloader:
    public ::testing::Test,
    public nx::utils::test::TestWithTemporaryDirectory
{
protected:
    virtual void SetUp() override
    {
        QFile resourceFile(kResourceFilePath);
        ASSERT_TRUE(resourceFile.open(QIODevice::ReadOnly));
        m_fileContents = resourceFile.readAll();

        m_filePath = testDataDir().toStdString() + "/test.txt";
        std::ofstream f(m_filePath.c_str(), std::ios_base::out | std::ios_base::binary);
        f << m_fileContents.toStdString();

        m_fileReadSize = std::max<std::size_t>(m_fileContents.size() / 3, 1);

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

        m_httpClient.setTimeouts(AsyncClient::kInfiniteTimeouts);
    }

    void whenRequestExistingFile()
    {
        whenRequestFile("/fs/test.txt");
    }

    void whenRequestFile(const std::string& path)
    {
        ASSERT_TRUE(m_httpClient.doGet(buildUrl(path)));
    }

    void thenRequestSucceeded()
    {
        thenRequestCompletedWith(StatusCode::ok);
    }

    void thenRequestCompletedWith(StatusCode::Value statusCode)
    {
        ASSERT_NE(nullptr, m_httpClient.response());
        ASSERT_EQ(statusCode, m_httpClient.response()->statusLine.statusCode);
    }

    void andContentLengthIsEqualToFileSize()
    {
        ASSERT_EQ(
            m_fileContents.size(),
            nx::utils::stoi(getHeaderValue(m_httpClient.response()->headers, "Content-Length")));
    }

    void andFileDownloaded()
    {
        const auto responseBody = m_httpClient.fetchEntireMessageBody();
        ASSERT_TRUE(responseBody);
        ASSERT_EQ(m_fileContents, *responseBody);
    }

private:
    std::string m_filePath;
    HttpClient m_httpClient{ssl::kAcceptAnyCertificate};
    TestHttpServer m_server;
    nx::Buffer m_fileContents;
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
    whenRequestFile(kResourceFileHttpPath);

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

} // namespace nx::network::http::server::handler::test
