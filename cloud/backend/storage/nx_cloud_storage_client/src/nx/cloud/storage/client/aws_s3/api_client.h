#pragma once

#include <string>

#include <nx/network/aio/async_operation_pool.h>
#include <nx/network/aio/basic_pollable.h>
#include <nx/network/buffer.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/http/http_async_client.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/std/filesystem.h>
#include <nx/utils/url.h>

namespace nx::cloud::storage::client::aws_s3 {

enum class ResultCode
{
    ok = 0,
    unauthorized,
    networkError,
    error,
    notImplemented,
};

constexpr std::string_view toString(ResultCode code)
{
    switch (code)
    {
        case ResultCode::ok: return "ok";
        case ResultCode::unauthorized: return "unauthorized";
        case ResultCode::networkError: return "networkError";
        case ResultCode::error: return "error";
        case ResultCode::notImplemented: return "notImplemented";
    }

    return "unknown";
}

struct Result
{
    ResultCode code = ResultCode::ok;
    std::string text;

    Result(ResultCode code):
        code(code),
        text(toString(code))
    {
    }

    Result(ResultCode code, std::string text):
        code(code),
        text(std::move(text))
    {
    }
};

//-------------------------------------------------------------------------------------------------

class NX_CLOUD_STORAGE_CLIENT_API ApiClient:
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
        const nx::utils::Url& url,
        const nx::network::http::Credentials& credentials);

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;

    void setTimeouts(const nx::network::http::AsyncClient::Timeouts& timeouts);

    void uploadFile(
        const std::string& destinationPath,
        nx::Buffer data,
        CommonHandler handler);

    /**
     * Uploads file specified by path into destinationPath in S3.
     */
    void uploadFile(
        const std::string& destinationPath,
        const std::filesystem::path& path,
        CommonHandler handler);

    void downloadFile(
        const std::string& path,
        DownloadHandler handler);

    /**
     * Downloads file from path and saves to local filesystem under the destinationPath.
     */
    void downloadFile(
        const std::string& path,
        const std::filesystem::path& destinationPath,
        CommonHandler handler);

protected:
    virtual void stopWhileInAioThread() override;

private:
    using RequestPool = nx::network::aio::AsyncOperationPool<nx::network::http::AsyncClient>;

    const nx::utils::Url m_url;
    RequestPool m_requestPool;
    nx::network::http::AsyncClient::Timeouts m_timeouts;

    void handleUploadResult(
        std::unique_ptr<nx::network::http::AsyncClient> httpClient,
        CommonHandler userHandler);

    void handleDownloadResult(
        std::unique_ptr<nx::network::http::AsyncClient> httpClient,
        DownloadHandler userHandler);

    ResultCode getResultCode(const nx::network::http::AsyncClient& httpClient) const;
};

} // namespace nx::cloud::storage::client::aws_s3
