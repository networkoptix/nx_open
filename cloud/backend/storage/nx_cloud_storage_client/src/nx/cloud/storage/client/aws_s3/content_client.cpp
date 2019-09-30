#include "content_client.h"

#include <nx/utils/log/log_message.h>
#include <nx/utils/string.h>
#include <nx/utils/time.h>

#include "utils.h"

namespace nx::cloud::storage::client::aws_s3 {

ContentClient::ContentClient(
    const std::string& /*storageClientId*/,
    const std::string& awsRegion,
    const nx::utils::Url& url,
    const nx::cloud::aws::Credentials& credentials)
    :
    m_awsClient(
        awsRegion,
        url,
        credentials)
{
    bindToAioThread(getAioThread());
}

void ContentClient::bindToAioThread(network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    m_awsClient.bindToAioThread(aioThread);
}

std::unique_ptr<AbstractUploader> ContentClient::startUpload(
    const std::string& /*deviceId*/,
    int /*streamIndex*/,
    std::chrono::system_clock::time_point /*timestamp*/)
{
    // TODO
    return nullptr;
}

void ContentClient::uploadMediaChunk(
    const std::string& deviceId,
    int streamIndex,
    std::chrono::system_clock::time_point timestamp,
    const nx::Buffer& data,
    Handler handler)
{
    const auto filePath = generateChunkPath(deviceId, streamIndex, timestamp);

    m_awsClient.uploadFile(
        filePath,
        std::move(data),
        [handler = std::move(handler)](nx::cloud::aws::Result result)
        {
            handler(toResultCode(result));
        });
}

void ContentClient::getChunkLog(
    std::chrono::system_clock::time_point /*lastKnownTimestamp*/,
    GetChunkLogHandler handler)
{
    handler(ResultCode::notImplemented, std::vector<ChunkLogEntry>());
    // TODO
}

void ContentClient::downloadChunk(
    const std::string& deviceId,
    int streamIndex,
    std::chrono::system_clock::time_point timestamp,
    DownloadHandler handler)
{
    const auto filePath = generateChunkPath(deviceId, streamIndex, timestamp);

    m_awsClient.downloadFile(
        filePath,
        [handler = std::move(handler)](
            nx::cloud::aws::Result result,
            nx::Buffer chunkData)
        {
            handler(toResultCode(result), std::move(chunkData));
        });
}

void ContentClient::removeChunk(
    const std::string& /*deviceId*/,
    int /*streamIndex*/,
    std::chrono::system_clock::time_point /*timestamp*/,
    Handler handler)
{
    handler(ResultCode::notImplemented);
    // TODO
}

void ContentClient::saveDeviceDescription(
    const std::string& cameraId,
    const DeviceDescription& data,
    Handler handler)
{
    m_awsClient.uploadFile(
        generateCameraInfoFilePath(cameraId),
        nx::Buffer::fromStdString(toString(data)),
        [handler = std::move(handler)](nx::cloud::aws::Result result)
        {
            handler(toResultCode(result));
        });
}

void ContentClient::getDeviceDescription(
    const std::string& cameraId,
    GetDeviceDescriptionHandler handler)
{
    m_awsClient.downloadFile(
        generateCameraInfoFilePath(cameraId),
        [handler = std::move(handler)](
            nx::cloud::aws::Result result,
            nx::Buffer data)
        {
            handler(
                toResultCode(result),
                fromString<DeviceDescription>(std::string_view(data.data(), data.size())));
        });
}

std::string ContentClient::generateChunkPath(
    const std::string& cameraId,
    int streamIndex,
    std::chrono::system_clock::time_point timestamp)
{
    const std::time_t unixTimestamp = std::chrono::system_clock::to_time_t(timestamp);
    std::tm tm;
    memset(&tm, 0, sizeof(tm));
    gmtime_r(&unixTimestamp, &tm);

    return nx::utils::buildString(
        "/camera/", cameraId, "/", streamIndex, "/",
        tm.tm_year + 1900, "/", tm.tm_mon + 1, "/", tm.tm_mday, "/", tm.tm_hour, "/",
        unixTimestamp, ".mkv");
}

std::string ContentClient::generateCameraInfoFilePath(
    const std::string& cameraId)
{
    return nx::utils::buildString("/camera/", cameraId, "/info.txt");
}

void ContentClient::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();

    m_awsClient.pleaseStopSync();
}

} // namespace nx::cloud::storage::client::aws_s3
