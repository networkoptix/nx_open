#pragma once

#include <optional>
#include <string>

#include <nx/network/aio/async_operation_pool.h>
#include <nx/network/aio/basic_pollable.h>
#include <nx/network/buffer.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/http/http_async_client.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/url.h>

#include <nx/cloud/aws/base_api_client.h>

#include "list_bucket_request.h"

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
        const std::string& awsRegion,
        const nx::utils::Url& url,
        const Credentials& credentials);
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
    void getLocation(nx::utils::MoveOnlyFunc<void(Result, std::string /*aws region*/)> handler);

    /**
     * Implementation of the GET Bucket(List Objects) v2 api call:
     * https://docs.aws.amazon.com/AmazonS3/latest/API/v2-RESTBucketGET.html
     */
    void listBucket(
        const ListBucketRequest& request,
        nx::utils::MoveOnlyFunc<void(Result, ListBucketResult)> handler);

private:
    std::tuple<nx::String, bool> calculateAuthorizationHeader(
        const network::http::Request& request,
        const Credentials& credentials,
        const std::string& region,
        const std::string& service) override;
};

} // namespace nx::cloud::aws::s3
