#include "api_client.h"

#include <nx/network/url/url_builder.h>

#include "nx/cloud/aws/format_query.h"

namespace nx::cloud::aws::sts {

namespace {

static constexpr char kService[] = "sts";

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
        nx::network::http::Method::post,
        nx::network::url::Builder(m_url).setPath("/").setQuery(buildQuery(request)),
        std::move(handler));
}

QString ApiClient::buildQuery(const AssumeRoleRequest& request) const
{
    static constexpr char kRequiredQueries[] = "Action=AssumeRole&Version=2011-06-15";

    QString query;
    query.reserve(1024);

    query += kRequiredQueries;
    if (!request.roleArn.empty())
        query += formatQuery("&RoleArn", request.roleArn);
    if (!request.roleSessionName.empty())
        query += formatQuery("&RoleSessionName", request.roleSessionName);
    int i = 0;
    for (const auto& policyArn : request.policyArns)
    {
        if (!policyArn.empty())
        {
            query += formatQuery(
                std::string("&PolicyArns.member.").append(std::to_string(++i)),
                policyArn);
        }
    }
    if (!request.policy.empty())
        query += formatQuery("&Policy", request.policy);
    if (request.durationSeconds > 0)
        query += formatQuery("&DurationSeconds", request.durationSeconds);
    if (!request.externalId.empty())
        query += formatQuery("&ExternalId", request.externalId);
    if (!request.serialNumber.empty())
        query += formatQuery("&SerialNumer", request.serialNumber);
    if (!request.tokenCode.empty())
        query += formatQuery("&TokenCode", request.tokenCode);

    return query;
}






}
