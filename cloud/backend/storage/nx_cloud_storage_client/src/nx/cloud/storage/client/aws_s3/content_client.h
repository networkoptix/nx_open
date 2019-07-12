#pragma once

#include <nx/network/http/auth_tools.h>
#include <nx/utils/url.h>

#include "../abstract_content_client.h"

namespace nx::cloud::storage::client::aws_s3 {

class ContentClient:
    public AbstractContentClient
{
public:
    ContentClient(
        const std::string& storageClientId,
        const nx::utils::Url& url,
        const nx::network::http::Credentials& credentials);

    virtual void uploadMediaChunk(
        const std::string& deviceId,
        int streamIndex,
        std::chrono::system_clock::time_point timestamp,
        const nx::Buffer& data,
        Handler handler) override;

    virtual void getChunkLog(
        std::chrono::system_clock::time_point lastKnownTimestamp,
        GetChunkLogHandler handler) override;

    virtual void downloadChunk(
        const std::string& deviceId,
        int streamIndex,
        std::chrono::system_clock::time_point timestamp,
        DownloadHandler handler) override;

    virtual void removeChunk(
        const std::string& deviceId,
        int streamIndex,
        std::chrono::system_clock::time_point timestamp,
        Handler handler) override;

    virtual void saveDeviceDescription(
        const std::string& cameraId,
        const DeviceDescription& data,
        Handler handler) override;

    virtual void getDeviceDescription(
        const std::string& cameraId,
        GetDeviceDescriptionHandler handler) override;
};

} // namespace nx::cloud::storage::client::aws_s3
