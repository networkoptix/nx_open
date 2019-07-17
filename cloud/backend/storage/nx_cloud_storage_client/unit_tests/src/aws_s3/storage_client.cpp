#include <gtest/gtest.h>

#include <nx/cloud/aws/test_support/aws_s3_emulator.h>
#include <nx/cloud/storage/client/aws_s3/storage_client.h>

namespace nx::cloud::storage::client::aws_s3::test {

class AwsS3StorageClient:
    public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
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
        // TODO
    }

    void thenMediaChunkIsUploaded()
    {
        // m_awsS3.getFile();
        // TODO
    }

    void andMediaChunkIsFoundUnderExpectedPath()
    {
        // TODO
    }

private:
    struct Chunk
    {
        const std::string deviceId;
        int streamIndex = 0;
        std::chrono::system_clock::time_point timestamp;
    };

    aws_s3::StorageClient m_client;
    aws::test::AwsS3Emulator m_awsS3;
    Chunk m_lastUploadedChunk;

    std::string generateChunkPath(const Chunk& chunk)
    {
        // TODO
        return std::string();
    }
};

TEST_F(AwsS3StorageClient, uploads_file)
{
    whenUploadMediaChunk();

    thenMediaChunkIsUploaded();
    andMediaChunkIsFoundUnderExpectedPath();
}

} // namespace nx::cloud::storage::client::aws_s3::test
