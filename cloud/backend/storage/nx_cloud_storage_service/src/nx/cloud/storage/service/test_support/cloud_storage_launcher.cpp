#include "cloud_storage_launcher.h"

namespace nx::cloud::storage::service::test {

CloudStorageLauncher::CloudStorageLauncher()
{
    addArg("-http/endpoints", "127.0.0.1:0");
}

nx::utils::Url CloudStorageLauncher::httpUrl() const
{
    return lm("http://%1").arg(moduleInstance()->httpEndpoints().front());
}

} // namespace nx::cloud::storage::service::test