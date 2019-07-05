#include "api_client.h"

namespace nx::cloud::storage::client::aws_s3 {

ApiClient::ApiClient(
    const std::string& /*storageClientId*/,
    const nx::utils::Url& /*url*/,
    const nx::network::http::Credentials& /*credentials*/)
{
    // TODO
}

void ApiClient::uploadFile(
    const std::string& /*destinationPath*/,
    const nx::Buffer& /*data*/,
    Handler handler)
{
    handler(Result(ResultCode::notImplemented));
    // TODO
}

void ApiClient::uploadFile(
    const std::string& /*destinationPath*/,
    const std::filesystem::path& /*data*/,
    Handler handler)
{
    handler(Result(ResultCode::notImplemented));
    // TODO
}

} // namespace nx::cloud::storage::client::aws_s3
