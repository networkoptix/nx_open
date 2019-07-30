#include "utils.h"

namespace nx::cloud::storage::service::utils {

namespace {

static constexpr char kBucketUrlTemplate[] = "http://%1.s3.amazonaws.com";
static constexpr char kS3HostSuffix[] = ".s3.amazonaws.com";

} // namespace

nx::utils::Url bucketUrl(const std::string& bucketName)
{
    return lm(kBucketUrlTemplate).arg(bucketName).toQString();
}

std::string parseBucketName(const nx::utils::Url& bucketUrl)
{
    QString host = bucketUrl.host();
    return host.left(host.indexOf(kS3HostSuffix)).toStdString();
}

} // namespace nx::cloud::storage::service::utils
