#include <gtest/gtest.h>

#include <nx/utils/thread/sync_queue.h>

#include <nx/cloud/storage/client/aws_s3/api_client.h>

namespace nx::cloud::storage::client::aws_s3::test {

class AwsS3Client:
    public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        m_client = std::make_unique<aws_s3::ApiClient>(
            "id",
            nx::utils::Url(),
            nx::network::http::Credentials());
    }

    void whenUploadFile()
    {
        m_client->uploadFile(
            "/test", nx::Buffer("Hello, world"),
            [this](auto result)
            {
                m_uploadResults.push(std::move(result));
            });
    }

    void thenFileIsUploaded()
    {
        ASSERT_EQ(ResultCode::ok, m_uploadResults.pop().code);

        // TODO
    }

private:
    std::unique_ptr<aws_s3::ApiClient> m_client;
    nx::utils::SyncQueue<aws_s3::Result> m_uploadResults;
};

TEST_F(AwsS3Client, DISABLED_uploads_file)
{
    whenUploadFile();
    thenFileIsUploaded();
}

} // namespace nx::cloud::storage::client::aws_s3::test
