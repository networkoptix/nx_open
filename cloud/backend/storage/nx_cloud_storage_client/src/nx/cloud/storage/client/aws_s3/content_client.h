#pragma once

#include <nx/network/http/auth_tools.h>
#include <nx/utils/url.h>

#include <nx/cloud/aws/s3/api_client.h>

#include "../abstract_content_client.h"

namespace nx::cloud::storage::client::aws_s3 {

class NX_CLOUD_STORAGE_CLIENT_API ContentClient:
    public AbstractContentClient
{
public:
    using base_type = AbstractContentClient;

    ContentClient(
        const std::string& storageClientId,
        const std::string& awsRegion,
        const nx::utils::Url& url,
        const nx::cloud::aws::Credentials& credentials);

    virtual void bindToAioThread(network::aio::AbstractAioThread* aioThread) override;

    virtual std::unique_ptr<AbstractUploader> startUpload(
        const std::string& deviceId,
        int streamIndex,
        std::chrono::system_clock::time_point timestamp) override;

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

    //---------------------------------------------------------------------------------------------

    static std::string generateChunkPath(
        const std::string& cameraId,
        int streamIndex,
        std::chrono::system_clock::time_point timestamp);

    static std::string generateCameraInfoFilePath(
        const std::string& cameraId);

protected:
    virtual void stopWhileInAioThread() override;

private:
    nx::cloud::aws::s3::ApiClient m_awsClient;
};

} // namespace nx::cloud::storage::client::aws_s3
