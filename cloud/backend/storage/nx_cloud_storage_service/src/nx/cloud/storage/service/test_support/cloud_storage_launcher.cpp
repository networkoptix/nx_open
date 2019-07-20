#include "cloud_storage_launcher.h"

#include <nx/network/http/http_types.h>
#include <nx/network/url/url_builder.h>

namespace nx::cloud::storage::service::test {

CloudStorageLauncher::CloudStorageLauncher()
{
    addArg("-http/endpoints", "127.0.0.1:0");
}

nx::utils::Url CloudStorageLauncher::httpUrl() const
{
    return network::url::Builder()
        .setScheme(network::http::kUrlSchemeName)
        .setEndpoint(moduleInstance()->httpEndpoints().front());
}

} // namespace nx::cloud::storage::service::test