#include "api_client.h"

#include <nx/network/url/url_builder.h>
#include <nx/utils/url_query.h>

namespace nx::cloud::aws::sts {

namespace {

static constexpr char kService[] = "sts";

nx::utils::UrlQuery buildQuery(const AssumeRoleRequest& request)
{
    nx::utils::UrlQuery query;
    query.add("Action", "AssumeRole").add("Version", "2011-06-15");

    if (!request.roleArn.empty())
        query.add("RoleArn", request.roleArn);
    if (!request.roleSessionName.empty())
        query.add("RoleSessionName", request.roleSessionName);
    int i = 0;
    for (const auto& policyArn : request.policyArns)
    {
        if (!policyArn.empty())
        {
            query.add(
                QString("PolicyArns.member.") + QString::number(++i),
                policyArn);
        }
    }
    if (!request.policy.empty())
        query.add("Policy", request.policy);
    if (request.durationSeconds > 0)
        query.add("DurationSeconds", request.durationSeconds);
    if (!request.externalId.empty())
        query.add("ExternalId", request.externalId);
    if (!request.serialNumber.empty())
        query.add("SerialNumber", request.serialNumber);
    if (!request.tokenCode.empty())
        query.add("TokenCode", request.tokenCode);

    return query;
}

} // namespace

ApiClient::ApiClient(
    const std::string& awsRegion,
    const nx::utils::Url& url,
    const nx::network::http::Credentials& credentials)
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
        nx::network::url::Builder(m_url).setPath("/").setQuery(buildQuery(request)),
        std::move(handler));
}

}
