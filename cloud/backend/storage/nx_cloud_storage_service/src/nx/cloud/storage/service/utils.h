#include <nx/utils/url.h>

namespace nx::cloud::storage::service::utils {

nx::utils::Url bucketUrl(const std::string& bucketName);

nx::utils::Url storageUrl(const std::string& bucketName, const std::string& storageId);

std::string bucketName(const nx::utils::Url& bucketUrl);

std::string storageFolder(const nx::utils::Url& bucketUrl);

} // namespace nx::cloud::storage::service::utils