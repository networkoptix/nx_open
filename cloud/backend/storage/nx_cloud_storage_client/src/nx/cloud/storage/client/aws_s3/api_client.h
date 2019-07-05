#pragma once

#include <string>

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/buffer.h>
#include <nx/network/http/auth_tools.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/std/filesystem.h>
#include <nx/utils/url.h>

namespace nx::cloud::storage::client::aws_s3 {

enum class ResultCode
{
    ok = 0,
    error,
    notImplemented,
};

constexpr std::string_view toString(ResultCode code)
{
    switch (code)
    {
        case ResultCode::ok:
            return "ok";

        case ResultCode::error:
            return "error";

        case ResultCode::notImplemented:
            return "notImplemented";
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

    // TODO
};

//-------------------------------------------------------------------------------------------------

class NX_CLOUD_STORAGE_CLIENT_API ApiClient:
    public nx::network::aio::BasicPollable
{
public:
    using Handler = nx::utils::MoveOnlyFunc<void(Result)>;

    ApiClient(
        const std::string& storageClientId,
        const nx::utils::Url& url,
        const nx::network::http::Credentials& credentials);

    void uploadFile(
        const std::string& destinationPath,
        const nx::Buffer& data,
        Handler handler);

    void uploadFile(
        const std::string& destinationPath,
        const std::filesystem::path& data,
        Handler handler);
};

} // namespace nx::cloud::storage::client::aws_s3
