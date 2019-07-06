#include <gtest/gtest.h>

#include <nx/cloud/storage/client/aws_s3/storage_client.h>

namespace nx::cloud::storage::client::aws_s3::test {

class AwsS3StorageClient:
    public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        // TODO
    }

    void whenUploadFile()
    {
        // TODO
    }

    void thenFileIsUploaded()
    {
        // TODO
    }

private:
    aws_s3::StorageClient m_client;
};

TEST_F(AwsS3StorageClient, DISABLED_uploads_file)
{
    whenUploadFile();
    thenFileIsUploaded();
}

} // namespace nx::cloud::storage::client::aws_s3::test
