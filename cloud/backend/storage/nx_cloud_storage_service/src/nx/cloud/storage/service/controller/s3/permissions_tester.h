#pragma once

#include <nx/network/aio/basic_pollable.h>

#include <nx/cloud/aws/s3/api_client.h>
#include <nx/network/http/auth_tools.h>

#include <nx/network/cloud/storage/service/api/result.h>

namespace nx::cloud::storage::service {

namespace conf { struct Aws; }

namespace controller::s3 {

class PermissionsTester:
    public nx::network::aio::BasicPollable
{
    using base_type = nx::network::aio::BasicPollable;

public:
    PermissionsTester(
        const network::http::Credentials& credentials,
        const nx::utils::Url& bucketUrl);
    ~PermissionsTester();

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

    void testFailed(std::string_view operation, const aws::Result& result);

private:
    const network::http::Credentials m_credentials;
    nx::utils::Url m_url;
    std::string m_bucketLocation;
    std::unique_ptr<aws::s3::ApiClient> m_s3Client;
    nx::utils::MoveOnlyFunc<void(api::Result, std::string)> m_handler;
};

} // namespace controller::s3
} // namespace nx::cloud::storage::service