#pragma once

#include <nx/network/aio/basic_pollable.h>

#include <nx/cloud/aws/s3/api_client.h>
#include <nx/network/http/auth_tools.h>

#include <nx/cloud/storage/service/api/result.h>

namespace nx::cloud::storage::service {

namespace conf { struct Aws; }

namespace controller::s3 {

class PermissionsTester:
    public nx::network::aio::BasicPollable
{
    using base_type = nx::network::aio::BasicPollable;

public:
    /**
     * @param credentials the credentials of the s3 bucket at bucketUrl
     * @param bucketUrl the url of the s3 bucket, e.g. "http://exampleBucket.s3.amazonaws.com"
     */
    PermissionsTester(
        const nx::cloud::aws::Credentials& credentials,
        const nx::utils::Url& bucketUrl);
    ~PermissionsTester();

    /**
     * Resolves the location of the S3 bucket and performs a file upload, download, and deletion
     * with a small amount data using credentials from the constructor.
     * Upon successful completion, the location of the bucket, which needs to be resolved to do the
     * test, is given to the caller.
     */
    void doTest(nx::utils::MoveOnlyFunc<void(api::Result, std::string /*location*/)> handler);

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;

private:
    virtual void stopWhileInAioThread() override;

    void initializeS3Client();

    void onLocationResolved(aws::Result result, std::string location);

    void doPermissionsTest();
    void onUploadFileDone(aws::Result result);
    void onDownloadFileDone(aws::Result result, nx::Buffer fileData);
    void onDeleteFileDone(aws::Result result);

    void testFailed(const char* operation, const aws::Result& result);

private:
    const nx::cloud::aws::Credentials& m_credentials;
    const nx::utils::Url m_url;
    std::string m_bucketLocation;
    std::unique_ptr<aws::s3::ApiClient> m_s3Client;
    nx::utils::MoveOnlyFunc<void(api::Result, std::string)> m_handler;
};

} // namespace controller::s3
} // namespace nx::cloud::storage::service