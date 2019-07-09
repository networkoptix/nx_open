#include "content_client.h"

namespace nx::cloud::storage::client::aws_s3 {

ContentClient::ContentClient(
    const std::string& /*storageClientId*/,
    const nx::utils::Url& /*url*/,
    const nx::network::http::Credentials& /*credentials*/)
{
    // TODO
}

void ContentClient::uploadMediaChunk(
    const std::string& /*deviceId*/,
    std::chrono::system_clock::time_point /*timestamp*/,
    const nx::Buffer& /*data*/,
    Handler handler)
{
    handler(ResultCode::notImplemented);
    // TODO
}

void ContentClient::getChunkLog(
    std::chrono::system_clock::time_point /*lastKnownTimestamp*/,
    GetChunkLogHandler handler)
{
    handler(ResultCode::notImplemented, std::vector<ChunkLogEntry>());
    // TODO
}

void ContentClient::downloadChunk(
    const std::string& /*deviceId*/,
    std::chrono::system_clock::time_point /*timestamp*/,
    DownloadHandler handler)
{
    handler(ResultCode::notImplemented, nx::Buffer());
    // TODO
}

void ContentClient::removeChunk(
    const std::string& /*deviceId*/,
    std::chrono::system_clock::time_point /*timestamp*/,
    Handler handler)
{
    handler(ResultCode::notImplemented);
    // TODO
}

void ContentClient::uploadFile(
    const std::string& /*destinationPath*/,
    const nx::Buffer& /*data*/,
    Handler handler)
{
    handler(ResultCode::notImplemented);
    // TODO
}

void ContentClient::downloadFile(
    const std::string& /*path*/,
    DownloadHandler handler)
{
    handler(ResultCode::notImplemented, nx::Buffer());
    // TODO
}

} // namespace nx::cloud::storage::client::aws_s3
