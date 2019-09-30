#include "utils.h"

#include <nx/network/url/url_builder.h>

namespace nx::cloud::storage::service::utils {

namespace {

static constexpr char kBucketUrlTemplate[] = "http://%1.s3.amazonaws.com";
static constexpr char kS3HostSuffix[] = ".s3.amazonaws.com";

} // namespace

nx::utils::Url bucketUrl(const std::string& bucketName)
{
    return lm(kBucketUrlTemplate).arg(bucketName).toQString();
}

nx::utils::Url storageUrl(const std::string& bucketName, const std::string& storageId)
{
    return network::url::Builder(bucketUrl(bucketName)).appendPath("/").appendPath(storageId);
}

std::string bucketName(const nx::utils::Url& bucketUrl)
{
    QString host = bucketUrl.host();
    return host.left(host.indexOf(kS3HostSuffix)).toStdString();
}

std::string storageFolder(const nx::utils::Url& bucketUrl)
{
    std::string path = bucketUrl.path().toStdString();
    while (path.front() == '/')
        path.erase(0, 1);
    return path;
}

} // namespace nx::cloud::storage::service::utils
