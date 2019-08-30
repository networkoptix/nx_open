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

        ASSERT_TRUE(m_awsS3.bindAndListen(nx::network::SocketAddress::anyPrivateAddress));

        // TODO: Enable authentication.

        std::promise<ResultCode> done;
        m_client.open(
            "test_client",
            m_awsS3.baseApiUrl(),
            nx::network::http::Credentials(),
            [&done](auto resultCode) { done.set_value(resultCode); });
        ASSERT_EQ(ResultCode::ok, done.get_future().get());
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
        // TODO
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
        // TODO
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
    nx::utils::SyncQueue<ResultCode> m_cameraInfoSaveResults;

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
                return std::make_pair(
                    nx::utils::generateRandomName(7).toStdString(),
                    nx::utils::generateRandomName(7).toStdString());
            });
    }
};

TEST_F(AwsS3StorageClient, uploads_file)
{
    whenUploadMediaChunk();
    thenMediaChunkIsUploadedUnderExpectedPath();
}

TEST_F(AwsS3StorageClient, DISABLED_saves_camera_info)
{
    whenSaveRandomCameraInfo();
    thenCameraInfoIsSaved();
}

TEST_F(AwsS3StorageClient, DISABLED_downloads_camera_info)
{
    whenSaveRandomCameraInfo();
    whenDownloadCameraInfo();
    thenExpectedCameraInfoIsDownloaded();
}

} // namespace nx::cloud::storage::client::aws_s3::test
