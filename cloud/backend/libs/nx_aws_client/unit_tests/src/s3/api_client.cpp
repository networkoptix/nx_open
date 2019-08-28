#include <gtest/gtest.h>

#include <nx/network/url/url_builder.h>
#include <nx/utils/thread/sync_queue.h>

#include <nx/cloud/aws/s3/api_client.h>
#include <nx/cloud/aws/test_support/aws_s3_emulator.h>

namespace nx::cloud::aws::s3::test {

class AwsS3Client:
    public ::testing::Test
{
    static constexpr char kExistingFilePath[] = "/s3_bucket/existing/file/path.ext";
    static constexpr char kExistingFileBody[] = "How am I alive?";

protected:
    virtual void SetUp() override
    {
        m_awsS3Emulator = std::make_unique<AwsS3Emulator>(s3::test::randomS3Location());

        ASSERT_TRUE(m_awsS3Emulator->bindAndListen(
            nx::network::SocketAddress::anyPrivateAddress));

        m_credentials.username = nx::utils::generateRandomName(7);
        m_credentials.authToken.setPassword(nx::utils::generateRandomName(7));
        m_awsS3Emulator->enableAthentication(std::regex(".*"), m_credentials);

        m_awsS3Emulator->saveOrReplaceFile(kExistingFilePath, kExistingFileBody);

        m_client = std::make_unique<aws::s3::ApiClient>(
            "us-east-1",
            nx::network::url::Builder()
                .setScheme(nx::network::http::kUrlSchemeName)
                .setEndpoint(m_awsS3Emulator->serverAddress()),
            m_credentials);

        m_client->setTimeouts(nx::network::http::AsyncClient::kInfiniteTimeouts);
    }

    void whenUploadFile()
    {
        m_lastUploadedFilePath.path = "/test";
        m_lastUploadedFilePath.contents = "Hello, world";

        m_client->uploadFile(
            m_lastUploadedFilePath.path, m_lastUploadedFilePath.contents,
            [this](auto result)
            {
                m_uploadResults.push(std::move(result));
            });
    }

    void whenDownloadFile()
    {
        m_client->downloadFile(
            kExistingFilePath,
            [this](auto result, auto fileContents)
            {
                m_downloadResults.push(std::make_tuple(result, std::move(fileContents)));
            });
    }

    void whenDeleteFile()
    {
        m_client->deleteFile(
            kExistingFilePath,
            [this](auto result) { m_deleteResults.push(result); });
    }

    void whenGetBucketLocation()
    {
        m_client->getLocation(
            [this](auto result, auto location)
            {
                m_getLocationResults.push(std::make_tuple(result, location));
            });
    }

    void thenUploadCompletesSuccessfully()
    {
        ASSERT_EQ(ResultCode::ok, m_uploadResults.pop().code());
    }

    void andFileIsUploaded()
    {
        const auto contents = m_awsS3Emulator->getFile(m_lastUploadedFilePath.path);
        ASSERT_TRUE(contents);
        ASSERT_EQ(m_lastUploadedFilePath.contents, *contents);
    }

    void thenDownloadIsDownloaded()
    {
        auto [result, fileContents] = m_downloadResults.pop();
        ASSERT_EQ(ResultCode::ok, result.code());
        ASSERT_EQ(kExistingFileBody, fileContents);
    }

    void thenFileIsDeleted()
    {
        const auto result = m_deleteResults.pop();
        ASSERT_EQ(ResultCode::ok, result.code());

        ASSERT_FALSE(m_awsS3Emulator->getFile(kExistingFilePath));
    }

    void thenLocationIsGotten()
    {
        auto [result, location] = m_getLocationResults.pop();
        ASSERT_EQ(ResultCode::ok, result.code());

        ASSERT_EQ(m_awsS3Emulator->location(), location);
    }

private:
    struct File
    {
        std::string path;
        nx::Buffer contents;
    };

    std::unique_ptr<AwsS3Emulator> m_awsS3Emulator;
    nx::cloud::aws::Credentials m_credentials;
    std::unique_ptr<aws::s3::ApiClient> m_client;
    nx::utils::SyncQueue<Result> m_uploadResults;
    nx::utils::SyncQueue<std::tuple<Result, nx::Buffer>> m_downloadResults;
    nx::utils::SyncQueue<Result> m_deleteResults;
    nx::utils::SyncQueue<std::tuple<Result, std::string>> m_getLocationResults;
    File m_lastUploadedFilePath;
};

TEST_F(AwsS3Client, uploads_file)
{
    whenUploadFile();

    thenUploadCompletesSuccessfully();
    andFileIsUploaded();
}

TEST_F(AwsS3Client, downloads_file)
{
    whenDownloadFile();
    thenDownloadIsDownloaded();
}

TEST_F(AwsS3Client, deletes_file)
{
    whenDeleteFile();
    thenFileIsDeleted();
}

TEST_F(AwsS3Client, gets_location)
{
    whenGetBucketLocation();
    thenLocationIsGotten();
}

} // namespace nx::cloud::aws::s3::test
