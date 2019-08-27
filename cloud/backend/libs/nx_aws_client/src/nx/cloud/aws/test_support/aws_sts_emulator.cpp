#include "aws_sts_emulator.h"

#include <nx/network/http/buffer_source.h>
#include <nx/network/url/url_builder.h>

#include "nx/cloud/aws/http_request_paths.h"

namespace nx::cloud::aws::sts::test {

namespace {

static const AssumedRoleUser kAssumedRoleUser{
    "ARO123EXAMPLE123:",
    "arn:aws:sts::123456789012:assumed-role/demo/"
};

} // namespace

AwsStsEmulator::AwsStsEmulator()
{
    registerHttpApi();
}

bool AwsStsEmulator::bindAndListen()
{
    return m_server.bindAndListen();
}

nx::utils::Url AwsStsEmulator::url() const
{
    return network::url::Builder()
        .setScheme(network::http::kUrlSchemeName)
        .setEndpoint(m_server.serverAddress());
}

std::optional<AssumeRoleResult> AwsStsEmulator::getAssumedRole(const std::string& accessKeyId)
{
    QnMutexLocker lock(&m_mutex);
    auto it = m_assumedRoles.find(accessKeyId);
    return it == m_assumedRoles.end() ? std::nullopt : std::make_optional(it->second);
}

void AwsStsEmulator::registerHttpApi()
{
    using namespace std::placeholders;

    m_server.httpMessageDispatcher().registerRequestProcessorFunc(
        network::http::Method::get,
        http::kRootPath,
        std::bind(&AwsStsEmulator::assumeRole, this, _1, _2));
}

void AwsStsEmulator::assumeRole(
    network::http::RequestContext requestContext,
    network::http::RequestProcessedHandler completionHandler)
{
    AssumeRoleRequest assumeRoleRequest;
    if (!deserialize(requestContext.request.requestLine.url.query(), &assumeRoleRequest))
        return completionHandler(network::http::StatusCode::badRequest);

    AssumeRoleResult assumeRoleResult;
    assumeRoleResult.credentials = Credentials{
        QnUuid::createUuid().toSimpleString().toStdString(),
        QnUuid::createUuid().toSimpleString().toStdString(),
        QnUuid::createUuid().toSimpleString().toStdString(),
        std::string()
    };
    assumeRoleResult.assumedRoleUser = kAssumedRoleUser;
    assumeRoleResult.assumedRoleUser.arn.append(assumeRoleRequest.roleSessionName);
    assumeRoleResult.assumedRoleUser.assumedRoleId.append(assumeRoleRequest.roleSessionName);

    {
        QnMutexLocker lock(&m_mutex);
        m_assumedRoles[assumeRoleResult.credentials.accessKeyId] = assumeRoleResult;
    }

    network::http::RequestResult requestResult(network::http::StatusCode::ok);
    requestResult.dataSource = std::make_unique<network::http::BufferSource>(
        "application/xml",
        xml::serialized(assumeRoleResult));

    completionHandler(std::move(requestResult));
}

} // namespace nx::cloud::aws::sts::test
