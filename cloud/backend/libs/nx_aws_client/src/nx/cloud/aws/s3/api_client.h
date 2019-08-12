#pragma once

#include "../base_api_client.h"

#include <optional>
#include <string>

#include <nx/network/aio/async_operation_pool.h>
#include <nx/network/aio/basic_pollable.h>
#include <nx/network/buffer.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/http/http_async_client.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/url.h>

namespace nx::cloud::aws::s3 {

class NX_AWS_CLIENT_API ApiClient:
    public BaseApiClient
{
    using base_type = BaseApiClient;

public:
    using CommonHandler = nx::utils::MoveOnlyFunc<void(Result)>;
    using DownloadHandler = nx::utils::MoveOnlyFunc<void(Result, nx::Buffer)>;

    /**
     * @param url Should be in form https://BucketName.s3.amazonaws.com/{pathPrefix}.
     * Though, this class will not parse and use BucketName.
     */
    ApiClient(
        const std::string& storageClientId,
        const std::string& awsRegion,
        const nx::utils::Url& url,
        const nx::network::http::Credentials& credentials);
    virtual ~ApiClient();

    void uploadFile(
        const std::string& destinationPath,
        nx::Buffer data,
        CommonHandler handler);

    void downloadFile(
        const std::string& path,
        DownloadHandler handler);

    void deleteFile(
        const std::string& path,
        CommonHandler handler);

    /**
     * Implementation of https://docs.aws.amazon.com/AmazonS3/latest/API/RESTBucketGETlocation.html
     * Get the location of the bucket specified by the url https://BucketName.s3.amazonaws.com
     */
    void getLocation(nx::utils::MoveOnlyFunc<void(Result, std::string/*location*/)> handler);

    /**
     * Get the number of bytes being stored at https://BucketName.s3.amazonaws.com/{pathPrefix}.
     */
    void getBucketSize(nx::utils::MoveOnlyFunc<void(Result, int/*bytes*/)> handler);

private:
    std::tuple<nx::String, bool> calculateAuthorizationHeader(
        const network::http::Request& request,
        const network::http::Credentials& credentials,
        const std::string& region,
        const std::string& service) override;

    void getBucketSizeInternal(
        int runningTotalSize,
        const std::string& continuationToken,
        nx::utils::MoveOnlyFunc<void(Result, int/*bytes*/)> handler);

    QString formatQuery(QString key, QString value);
};

} // namespace nx::cloud::aws::s3
