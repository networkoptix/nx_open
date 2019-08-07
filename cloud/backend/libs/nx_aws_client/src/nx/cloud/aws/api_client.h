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

#include "api_types.h"

namespace nx::cloud::aws {

class NX_AWS_CLIENT_API ApiClient:
    public nx::network::aio::BasicPollable
{
    using base_type = nx::network::aio::BasicPollable;

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

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;

    void setTimeouts(const nx::network::http::AsyncClient::Timeouts& timeouts);

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

protected:
    virtual void stopWhileInAioThread() override;

private:
    using RequestPool = nx::network::aio::AsyncOperationPool<nx::network::http::AsyncClient>;

    const std::string m_awsRegion;
    const nx::utils::Url m_url;
    const nx::network::http::Credentials m_credentials;
    RequestPool m_requestPool;
    nx::network::http::AsyncClient::Timeouts m_timeouts;

    std::unique_ptr<network::http::AsyncClient> prepareHttpClient();

    void addAuthorizationToRequest(network::http::Request* request);

    template<typename Handler>
    void doAwsApiCall(
        const nx::network::http::Method::ValueType& method,
        const std::string& path,
        Handler handler,
        std::unique_ptr<nx::network::http::AbstractMsgBodySource> body = nullptr);

    template<typename Handler>
    void doAwsApiCall(const nx::network::http::Method::ValueType& method,
        const nx::utils::Url& url,
        Handler handler,
        std::unique_ptr<nx::network::http::AbstractMsgBodySource> body = nullptr);

    ResultCode getResultCode(const nx::network::http::AsyncClient& httpClient) const;

    void getBucketSizeInternal(
        int runningTotalSize,
        const std::string& continuationToken,
        nx::utils::MoveOnlyFunc<void(Result, int/*bytes*/)> handler);

    QString formatQuery(QString key, QString value);
};

} // namespace nx::cloud::aws
