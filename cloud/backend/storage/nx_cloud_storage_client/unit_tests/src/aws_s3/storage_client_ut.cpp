#include <chrono>
#include <ctime>
#include <future>

#include <gtest/gtest.h>

#include <nx/utils/thread/sync_queue.h>

#include <nx/cloud/aws/test_support/aws_s3_emulator.h>
#include <nx/cloud/storage/client/aws_s3/content_client.h>
#include <nx/cloud/storage/client/aws_s3/storage_client.h>

namespace nx::cloud::storage::client::aws_s3::test {

static constexpr char kTestChunkData[] = "testtesttest";

class AwsS3StorageClient:
    public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        m_cameraId = nx::utils::generateRandomName(7).toStdString();

        m_awsS3.setLocation("us-west-1");
        ASSERT_TRUE(m_awsS3.bindAndListen(nx::network::SocketAddress::anyPrivateAddress));

        m_credentials.username = nx::utils::generateRandomName(7);
        m_credentials.authToken = nx::network::http::PasswordAuthToken(
            nx::utils::generateRandomName(7));

        m_awsS3.enableAthentication(std::regex(".*"), m_credentials);
    }

    void givenInvalidAwsCredentials()
    {
        m_credentials.username = "hoi";
    }

    void givenUploadMediaChunk()
    {
        whenUploadMediaChunk();
    }

    void givenUploadedCameraInfo()
    {
        whenSaveRandomCameraInfo();
        thenCameraInfoIsSaved();
    }

    void whenOpenStorageClient()
    {
        m_client.open(
            "test_client",
            m_awsS3.baseApiUrl(),
            m_credentials,
            [this](auto resultCode) { m_openResults.push(resultCode); });
    }

    void whenUploadMediaChunk()
    {
        m_lastUploadedChunk.deviceId = "ddd";
        m_lastUploadedChunk.streamIndex = 1;
        m_lastUploadedChunk.timestamp = std::chrono::system_clock::now();
        m_lastUploadedChunk.data = kTestChunkData;

        std::promise<ResultCode> done;
        m_client.getContentClient()->uploadMediaChunk(
            m_lastUploadedChunk.deviceId,
            m_lastUploadedChunk.streamIndex,
            m_lastUploadedChunk.timestamp,
            m_lastUploadedChunk.data,
            [&done](ResultCode resultCode) { done.set_value(resultCode); });
        ASSERT_EQ(ResultCode::ok, done.get_future().get());
    }

    void whenSaveRandomCameraInfo()
    {
        generateRandomCameraInfo();

        m_client.getContentClient()->saveDeviceDescription(
            m_cameraId,
            m_cameraInfo,
            [this](auto result) { m_cameraInfoSaveResults.push(result); });
    }

    void whenDownloadCameraInfo()
    {
        m_client.getContentClient()->getDeviceDescription(
            m_cameraId,
            [this](auto&&... args)
            {
                m_cameraInfoDownloadResults.push(std::forward<decltype(args)>(args)...);
            });
    }

    void whenDownloadThatChunk()
    {
        m_client.getContentClient()->downloadChunk(
            m_lastUploadedChunk.deviceId,
            m_lastUploadedChunk.streamIndex,
            m_lastUploadedChunk.timestamp,
            [this](auto&&... args)
            {
                m_chunkDownloadResults.push(std::forward<decltype(args)>(args)...);
            });
    }

    void thenTheChunkIsDownloaded()
    {
        m_lastChunkDownloadResult = m_chunkDownloadResults.pop();

        ASSERT_EQ(ResultCode::ok, std::get<0>(m_lastChunkDownloadResult));
        ASSERT_EQ(m_lastUploadedChunk.data, std::get<1>(m_lastChunkDownloadResult));
    }

    void thenOpenFailed()
    {
        m_lastOpenResult = m_openResults.pop();
        ASSERT_NE(ResultCode::ok, m_lastOpenResult);
    }

    void thenOpenSucceeded()
    {
        m_lastOpenResult = m_openResults.pop();
        ASSERT_EQ(ResultCode::ok, m_lastOpenResult);
    }

    void thenMediaChunkIsUploadedUnderExpectedPath()
    {
        const auto file = m_awsS3.getFile(generateChunkPath(m_lastUploadedChunk));
        ASSERT_TRUE(file);
        ASSERT_EQ(m_lastUploadedChunk.data, *file);
    }

    void thenCameraInfoIsSaved()
    {
        ASSERT_EQ(ResultCode::ok, m_cameraInfoSaveResults.pop());

        const auto infoFile = m_awsS3.getFile(
            ContentClient::generateCameraInfoFilePath(m_cameraId));
        ASSERT_TRUE(infoFile);
    }

    void thenExpectedCameraInfoIsDownloaded()
    {
        const auto [result, description] = m_cameraInfoDownloadResults.pop();

        ASSERT_EQ(ResultCode::ok, result);
        ASSERT_EQ(m_cameraInfo, description);
    }

    void andResultCodeIs(ResultCode resultCode)
    {
        ASSERT_EQ(resultCode, m_lastOpenResult);
    }

    void andValidAwsRegionIsUsedToReferToBucket()
    {
        ASSERT_EQ(m_awsS3.location(), m_client.awsBucketRegion());
    }

private:
    struct Chunk
    {
        std::string deviceId;
        int streamIndex = 0;
        std::chrono::system_clock::time_point timestamp;
        nx::Buffer data;
    };

    aws_s3::StorageClient m_client;
    aws::s3::test::AwsS3Emulator m_awsS3;
    std::string m_cameraId;
    Chunk m_lastUploadedChunk;
    DeviceDescription m_cameraInfo;
    nx::utils::SyncQueue<ResultCode> m_openResults;
    nx::utils::SyncQueue<ResultCode> m_cameraInfoSaveResults;
    nx::utils::SyncQueue<std::tuple<ResultCode, DeviceDescription>> m_cameraInfoDownloadResults;
    nx::utils::SyncQueue<std::tuple<ResultCode, nx::Buffer /*fileData*/>> m_chunkDownloadResults;
    std::tuple<ResultCode, nx::Buffer /*fileData*/> m_lastChunkDownloadResult;
    ResultCode m_lastOpenResult = ResultCode::ok;
    nx::network::http::Credentials m_credentials;

    std::string generateChunkPath(const Chunk& chunk)
    {
        return ContentClient::generateChunkPath(
            chunk.deviceId,
            chunk.streamIndex,
            chunk.timestamp);
    }

    void generateRandomCameraInfo()
    {
        m_cameraInfo.resize(7);
        std::generate(
            m_cameraInfo.begin(), m_cameraInfo.end(),
            []()
            {
                return Parameter{
                    nx::utils::generateRandomName(7).toStdString(),
                    nx::utils::generateRandomName(7).toStdString()};
            });
    }
};

//-------------------------------------------------------------------------------------------------

TEST_F(AwsS3StorageClient, open_fails_on_invalid_credentials)
{
    givenInvalidAwsCredentials();

    whenOpenStorageClient();

    thenOpenFailed();
    andResultCodeIs(ResultCode::unauthorized);
}

TEST_F(AwsS3StorageClient, open_succeeds_on_valid_credentials)
{
    // Using valid credentials by default.
    whenOpenStorageClient();
    thenOpenSucceeded();
}

TEST_F(AwsS3StorageClient, open_resolves_aws_region)
{
    whenOpenStorageClient();

    thenOpenSucceeded();
    andValidAwsRegionIsUsedToReferToBucket();
}

//-------------------------------------------------------------------------------------------------

class AwsS3StorageContentClient:
    public AwsS3StorageClient
{
    using base_type = AwsS3StorageClient;

protected:
    virtual void SetUp() override
    {
        base_type::SetUp();

        whenOpenStorageClient();
        thenOpenSucceeded();
    }
};

//-------------------------------------------------------------------------------------------------
// Operations on media files.

TEST_F(AwsS3StorageContentClient, uploads_file)
{
    whenUploadMediaChunk();
    thenMediaChunkIsUploadedUnderExpectedPath();
}

TEST_F(AwsS3StorageContentClient, downloads_file)
{
    givenUploadMediaChunk();
    whenDownloadThatChunk();
    thenTheChunkIsDownloaded();
}

//-------------------------------------------------------------------------------------------------
// Camera info.

TEST_F(AwsS3StorageContentClient, camera_info_is_uploaded)
{
    whenSaveRandomCameraInfo();
    thenCameraInfoIsSaved();
}

TEST_F(AwsS3StorageContentClient, camera_info_is_downloaded)
{
    givenUploadedCameraInfo();
    whenDownloadCameraInfo();
    thenExpectedCameraInfoIsDownloaded();
}

} // namespace nx::cloud::storage::client::aws_s3::test
