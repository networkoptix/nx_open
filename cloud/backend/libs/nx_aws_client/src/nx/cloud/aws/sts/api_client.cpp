#include "api_client.h"

#include <nx/network/url/url_builder.h>

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

void ApiClient::addAuthorizationToRequest(network::http::Request* request)
{

}

QString ApiClient::buildQuery(const AssumeRoleRequest& request) const
{
    static constexpr char kRequiredQueries[] = "Action=AssumeRole&Version=2011-06-15";

    QString query;
    query.reserve(1024);

    query += kRequiredQueries;
    if (!request.roleArn.empty())
        query.append("&RoleArn=").append(request.roleArn.c_str());
    if (!request.roleSessionName.empty())
        query.append("&RoleSessionName=").append(request.roleSessionName.c_str());
    int i = 0;
    for (const auto& policyArn : request.policyArns)
    {
        if (!policyArn.empty())
        {
            query.append("&PolicyArns.member.").append(QString::number(++i))
                .append("=").append(policyArn.c_str());
        }
    }
    if (!request.policy.empty())
        query.append("&Policy=").append(request.policy.c_str());
    if (request.durationSeconds > 0)
        query.append("&DurationSeconds=").append(QString::number(request.durationSeconds));
    if (!request.externalId.empty())
        query.append("&ExternalId=").append(request.externalId.c_str());
    if (!request.serialNumber.empty())
        query.append("&SerialNumer=").append(request.serialNumber.c_str());
    if (!request.tokenCode.empty())
        query.append("&TokenCode=").append(request.tokenCode.c_str());

    return query;
}






}
