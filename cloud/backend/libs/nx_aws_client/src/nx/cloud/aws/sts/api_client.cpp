#include "api_client.h"

#include <nx/network/url/url_builder.h>
#include <nx/utils/url_query.h>

#include "nx/cloud/aws/http_request_paths.h"

namespace nx::cloud::aws::sts {

namespace {

static constexpr char kService[] = "sts";

} // namespace

ApiClient::ApiClient(
    const std::string& awsRegion,
    const nx::utils::Url& url,
    const aws::Credentials& credentials)
    :
    base_type(kService, awsRegion, url, credentials)
{
}

ApiClient::~ApiClient()
{
    pleaseStopSync();
}

void ApiClient::assumeRole(
    const AssumeRoleRequest& request,
    nx::utils::MoveOnlyFunc<void(Result, AssumeRoleResult)> handler)
{
    doAwsApiCall(
        nx::network::http::Method::get,
        nx::network::url::Builder(m_url).setPath(http::kRootPath).setQuery(serialize(request)),
        std::move(handler));
}

}
