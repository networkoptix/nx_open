#include <nx/utils/url.h>

namespace nx::cloud::storage::service::utils {

nx::utils::Url bucketUrl(const std::string& bucketName);

std::string parseBucketName(const nx::utils::Url& bucketUrl);

} // namespace nx::cloud::storage::service::utils