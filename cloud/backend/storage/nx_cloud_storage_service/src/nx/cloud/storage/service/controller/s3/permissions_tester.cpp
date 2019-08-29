#include "permissions_tester.h"

#include "nx/cloud/storage/service/settings.h"
#include "nx/cloud/storage/service/controller/utils.h"

namespace nx::cloud::storage::service::controller::s3 {

using namespace std::placeholders;

namespace {

static constexpr char kFileData[] = "permissionstest";

} // namespace

PermissionsTester::PermissionsTester(
    const nx::cloud::aws::Credentials& credentials,
    const nx::utils::Url& bucketUrl)
    :
    m_credentials(credentials),
    m_url(bucketUrl),
    m_bucketLocation(utils::kDefaultBucketRegion)
{
}

PermissionsTester::~PermissionsTester()
{
    pleaseStopSync();
}

void PermissionsTester::doTest(nx::utils::MoveOnlyFunc<void(api::Result, std::string)> handler)
{
    dispatch(
        [this, handler = std::move(handler)]() mutable
        {
            m_handler = std::move(handler);
            initializeS3Client();
            m_s3Client->getLocation(
                std::bind(&PermissionsTester::onLocationResolved, this, _1, _2));
        });
}

void PermissionsTester::bindToAioThread(nx::network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);
    if (m_s3Client)
        m_s3Client->bindToAioThread(aioThread);
}

void PermissionsTester::stopWhileInAioThread()
{
    m_s3Client.reset();
}

void PermissionsTester::initializeS3Client()
{
    m_s3Client = std::make_unique<aws::s3::ApiClient>(
        m_bucketLocation,
        m_url,
        m_credentials);
    m_s3Client->bindToAioThread(getAioThread());
}

void PermissionsTester::onLocationResolved(aws::Result result, std::string location)
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

void PermissionsTester::doPermissionsTest()
{
    m_s3Client->uploadFile(
        kFileData,
        kFileData,
        std::bind(&PermissionsTester::onUploadFileDone, this, _1));
}

void PermissionsTester::onUploadFileDone(aws::Result result)
{
    if (!result.ok())
        return testFailed("File upload", result);

    m_s3Client->downloadFile(
        kFileData,
        std::bind(&PermissionsTester::onDownloadFileDone, this, _1, _2));
}

void PermissionsTester::onDownloadFileDone(aws::Result result, nx::Buffer fileData)
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
        std::bind(&PermissionsTester::onDeleteFileDone, this, _1));
}

void PermissionsTester::onDeleteFileDone(aws::Result result)
{
    if (!result.ok())
        return testFailed("File deletion", result);

    nx::utils::swapAndCall(m_handler, api::Result(), m_bucketLocation);
}

void PermissionsTester::testFailed(const char* operation, const aws::Result& result)
{
    NX_VERBOSE(this, "%1 failed: %2, %3",
        operation, aws::toString(result.code()), result.text());
    nx::utils::swapAndCall(m_handler, utils::toResult(result), std::string());
}



} // namespace nx::cloud::storage::service::controller::s3
