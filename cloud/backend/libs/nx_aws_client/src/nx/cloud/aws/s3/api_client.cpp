#include "api_client.h"

#include <nx/network/http/buffer_source.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/log/log.h>

#include "nx/cloud/aws/http_request_paths.h"
#include "aws_signature_v4.h"
#include "http_request_paths.h"
#include "location_constraint.h"
#include "list_bucket_result.h"

namespace nx::cloud::aws::s3 {

namespace {

static constexpr char kAwsS3ServiceName[] = "s3";

} // namespace

ApiClient::ApiClient(
    const std::string& /*storageClientId*/,
    const std::string& awsRegion,
    const nx::utils::Url& url,
    const nx::network::http::Credentials& credentials)
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

void ApiClient::getBucketSize(
    nx::utils::MoveOnlyFunc<void(Result, int)> handler)
{
    getBucketSizeInternal(0, {}, std::move(handler));
}

std::tuple<nx::String, bool> nx::cloud::aws::s3::ApiClient::calculateAuthorizationHeader(
    const network::http::Request& request,
    const network::http::Credentials& credentials,
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

void ApiClient::getBucketSizeInternal(
    int runningTotalSize,
    const std::string& continuationToken,
    nx::utils::MoveOnlyFunc<void(Result, int)> handler)
{
    QString query(http::kListTypeQuery);
    auto path = m_url.path();
    if (!path.isEmpty())
    {
        query.append("&").append(
            formatQuery(http::kPrefix, path[0] == '/' && path.size() > 1 ? path.mid(1) : path));
    }
    if (!continuationToken.empty())
        query.append("&").append(formatQuery(http::kContinuationToken, continuationToken.c_str()));

    doAwsApiCall<ListBucketResult>(
        nx::network::http::Method::get,
        nx::network::url::Builder(m_url).setPath(aws::http::kRootPath).setQuery(query),
        [this, handler = std::move(handler), runningTotalSize](auto result, auto listBucketResult) mutable
    {
        if (!result.ok())
            return handler(std::move(result), 0);

        int size = runningTotalSize;
        for (const auto& content : listBucketResult.contents)
            size += content.size;

        if (!listBucketResult.isTruncated)
            return handler(ResultCode::ok, size);

        // if isTruncated is true, then nextContinuationToken should not be empty
        if (listBucketResult.nextContinuationToken.empty())
            return handler(Result(ResultCode::error, "Missing continuation token"), 0);

        getBucketSizeInternal(size, listBucketResult.nextContinuationToken, std::move(handler));
    });
}

QString ApiClient::formatQuery(QString key, QString value)
{
    QString s;
    s.reserve(key.size() + value.size() + 1);
    return s.append(key).append("=").append(value);
}

} // namespace nx::cloud::aws::s3
