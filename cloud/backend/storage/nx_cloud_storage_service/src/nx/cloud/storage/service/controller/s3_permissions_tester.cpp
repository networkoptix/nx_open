#include "s3_permissions_tester.h"

#include "nx/cloud/storage/service/settings.h"

namespace nx::cloud::storage::service::controller {

using namespace std::placeholders;

namespace {

static constexpr char kDefaultBucketLocation[] = "us-east-1";
static constexpr char kFileData[] = "s3permissionstest";

} // namespace

S3PermissionsTester::S3PermissionsTester(
    const network::http::Credentials& credentials,
    const nx::utils::Url& bucketUrl)
    :
    m_credentials(credentials),
    m_url(bucketUrl),
    m_bucketLocation(kDefaultBucketLocation)
{
}

S3PermissionsTester::~S3PermissionsTester()
{
    pleaseStopSync();
}

void S3PermissionsTester::doTest(nx::utils::MoveOnlyFunc<void(api::Result, std::string)> handler)
{
    dispatch(
        [this, handler = std::move(handler)]() mutable
        {
            m_handler = std::move(handler);
            initializeS3Client();
            m_s3Client->getLocation(
                std::bind(&S3PermissionsTester::onLocationResolved, this, _1, _2));
        });
}

void S3PermissionsTester::bindToAioThread(nx::network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);
    if (m_s3Client)
        m_s3Client->bindToAioThread(aioThread);
}

void S3PermissionsTester::stopWhileInAioThread()
{
    m_s3Client.reset();
}

void S3PermissionsTester::initializeS3Client()
{
    m_s3Client = std::make_unique<aws::ApiClient>(
        std::string() /*storageClientId*/,
        m_bucketLocation,
        m_url,
        m_credentials);
    m_s3Client->bindToAioThread(getAioThread());
}

void S3PermissionsTester::onLocationResolved(aws::Result result, std::string location)
{
    if (!result.ok())
        return testFailed("Location resolution", result);

    if (location == m_bucketLocation)
        return doPermissionsTest();

    // If location is different, S3 client must reinitialize with the correct region to perform
    // permissions test. Otherwise authorization will fail.

    m_bucketLocation = location;
    dispatch(
        [this]()
        {
            initializeS3Client();
            doPermissionsTest();
        });
}

void S3PermissionsTester::doPermissionsTest()
{
    m_s3Client->uploadFile(
        kFileData,
        kFileData,
        std::bind(&S3PermissionsTester::onUploadFileDone, this, _1));
}

void S3PermissionsTester::onUploadFileDone(aws::Result result)
{
    if (!result.ok())
        return testFailed("File upload", result);

    m_s3Client->downloadFile(
        kFileData,
        std::bind(&S3PermissionsTester::onDownloadFileDone, this, _1, _2));
}

void S3PermissionsTester::onDownloadFileDone(aws::Result result, nx::Buffer fileData)
{
    if (!result.ok())
        return testFailed("File download", result);

    if (fileData != kFileData)
    {
        std::string error =
            lm("Downloaded different data than what was uploaded. Expected %1, got %2")
                .args(kFileData, fileData).toStdString();
        return testFailed("File download", aws::Result(aws::ResultCode::error, std::move(error)));
    }

    m_s3Client->deleteFile(
        kFileData,
        std::bind(&S3PermissionsTester::onDeleteFileDone, this, _1));
}

void S3PermissionsTester::onDeleteFileDone(aws::Result result)
{
    if (!result.ok())
        return testFailed("File deletion", result);

    nx::utils::swapAndCall(m_handler, api::Result(), m_bucketLocation);
}

void S3PermissionsTester::testFailed(std::string_view operation, const aws::Result& result)
{
    NX_ERROR(this, "%1 failed: %2, %3",
        operation.data(), aws::toString(result.code()).data(), result.text());
    nx::utils::swapAndCall(
        m_handler,
        api::Result(api::ResultCode::awsApiError, result.text()),
        std::string());
}



} // namespace nx::cloud::storage::service::controller
