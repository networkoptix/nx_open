#include "api_client.h"

#include <nx/network/http/buffer_source.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/log/log.h>
#include <nx/utils/url_query.h>

#include "nx/cloud/aws/http_request_paths.h"
#include "aws_signature_v4.h"
#include "http_request_paths.h"
#include "location_constraint.h"

namespace nx::cloud::aws::s3 {

namespace {

static constexpr char kAwsS3ServiceName[] = "s3";

nx::utils::UrlQuery buildQuery(const ListBucketRequest& request)
{
    nx::utils::UrlQuery query;
    query.addQueryItem(http::kListType, http::kListTypeValue);

    if (!request.delimiter.empty())
        query.addQueryItem("delimiter", request.delimiter);
    if (!request.encodingType.empty())
        query.addQueryItem("encoding-type", request.encodingType);
    if (request.maxKeys > 0)
        query.addQueryItem("max-keys", request.maxKeys);
    if (!request.prefix.empty())
        query.addQueryItem("prefix", request.prefix);
    if (!request.continuationToken.empty())
        query.addQueryItem("continuation-token", request.continuationToken);
    if (!request.startAfter.empty())
        query.addQueryItem("start-after", request.startAfter);

    return query.addQueryItem("fetch-owner", request.fetchOwner);
}

} // namespace

ApiClient::ApiClient(
    const std::string& awsRegion,
    const nx::utils::Url& url,
    const Credentials& credentials)
    :
    base_type(kAwsS3ServiceName, awsRegion, url, credentials)
{
}

ApiClient::~ApiClient()
{
    pleaseStopSync();
}

void ApiClient::uploadFile(
    const std::string& destinationPath,
    nx::Buffer data,
    CommonHandler handler)
{
    doAwsApiCall(
        network::http::Method::put,
        destinationPath,
        [this, handler = std::move(handler)](auto httpClient) mutable
        {
            handler(getResultCode(*httpClient));
        },
        std::make_unique<network::http::BufferSource>(
            "application/octet-stream",
            std::move(data)));
}

void ApiClient::downloadFile(
    const std::string& path,
    DownloadHandler handler)
{
    doAwsApiCall(
        nx::network::http::Method::get,
        path,
        [this, handler = std::move(handler)](auto httpClient) mutable
        {
            const auto resultCode = getResultCode(*httpClient);
            return handler(Result(resultCode), httpClient->fetchMessageBodyBuffer());
        });
}

void ApiClient::deleteFile(
    const std::string& path,
    CommonHandler handler)
{
    doAwsApiCall(
        nx::network::http::Method::delete_,
        path,
        [this, handler = std::move(handler)](auto httpClient) mutable
        {
            handler(getResultCode(*httpClient));
        });
}

void ApiClient::getLocation(
    nx::utils::MoveOnlyFunc<void(Result, std::string)> handler)
{
    doAwsApiCall(
        nx::network::http::Method::get,
        nx::network::url::Builder(m_url).setPath(aws::http::kRootPath).setQuery(http::kLocation),
        [this, handler = std::move(handler)](auto httpClient)
        {
            LocationConstraint locationConstraint;
            auto messageBody = httpClient->fetchMessageBodyBuffer();
            if (!xml::deserialize(messageBody, &locationConstraint))
            {
                return handler(
                    Result(getResultCode(*httpClient), messageBody.constData()),
                    std::string());
            }

            handler(ResultCode::ok, std::move(locationConstraint.region));
        });
}

void ApiClient::listBucket(
    const ListBucketRequest& request,
    nx::utils::MoveOnlyFunc<void(Result, ListBucketResult)> handler)
{
    doAwsApiCall<ListBucketResult>(
        nx::network::http::Method::get,
        nx::network::url::Builder(m_url).setPath(aws::http::kRootPath).setQuery(buildQuery(request)),
        std::move(handler));
}

std::tuple<nx::String, bool> ApiClient::calculateAuthorizationHeader(
    const network::http::Request& request,
    const Credentials& credentials,
    const std::string& region,
    const std::string& service)
{
    SignatureCalculator calculator;
    return calculator.calculateAuthorizationHeader(
        request,
        credentials,
        region,
        service);
}

} // namespace nx::cloud::aws::s3
