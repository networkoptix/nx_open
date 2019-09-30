#pragma once

#include <chrono>
#include <string>
#include <vector>

#include <nx/network/aio/basic_pollable.h>
#include <nx/utils/move_only_func.h>

#include "result_code.h"
#include "types.h"

namespace nx::cloud::storage::client {

/**
 * Allows to upload files to the storage by chunks, without having the whole file in the memory.
 * Internally, chunks are queued and written in the same order they were provided to the write call.
 */
class NX_CLOUD_STORAGE_CLIENT_API AbstractUploader:
    public nx::network::aio::BasicPollable
{
public:
    virtual ~AbstractUploader() = default;

    /**
     * Buffers are queued internally, so the next write can be invoked without waiting
     * for the previous one to complete.
     */
    virtual void write(
        nx::Buffer data,
        nx::utils::MoveOnlyFunc<void(ResultCode)> handler) = 0;

    /**
     * This function should be called to finish the upload gracefully.
     * Otherwise, the resulting file may be incomplete.
     * @param handler Invoked after all data has been successfully uploaded.
     */
    virtual void finish(nx::utils::MoveOnlyFunc<void(ResultCode)> handler) = 0;
};

//-------------------------------------------------------------------------------------------------

/**
 * NOTE: Chunk log is modified automatically when uploading/removing chunks.
 */
class NX_CLOUD_STORAGE_CLIENT_API AbstractContentClient:
    public nx::network::aio::BasicPollable
{
public:
    using Handler = nx::utils::MoveOnlyFunc<void(ResultCode)>;

    using GetChunkLogHandler =
        nx::utils::MoveOnlyFunc<void(ResultCode, std::vector<ChunkLogEntry>)>;

    using DownloadHandler =
        nx::utils::MoveOnlyFunc<void(ResultCode, nx::Buffer /*fileData*/)>;

    using GetDeviceDescriptionHandler =
        nx::utils::MoveOnlyFunc<void(ResultCode, DeviceDescription /*data*/)>;

    //---------------------------------------------------------------------------------------------
    // Uploading chunks.

    /**
     * Upload file by pieces.
     */
    virtual std::unique_ptr<AbstractUploader> startUpload(
        const std::string& deviceId,
        int streamIndex,
        std::chrono::system_clock::time_point timestamp) = 0;

    /**
     * The default implemetation is a wrapper on top of AbstractContentClient::startUpload call.
     * The derived class may choose to reimplement it in a more efficient way.
     */
    virtual void uploadMediaChunk(
        const std::string& deviceId,
        int streamIndex,
        std::chrono::system_clock::time_point timestamp,
        const nx::Buffer& data,
        Handler handler);

    //---------------------------------------------------------------------------------------------
    // Fetching chunks.

    virtual void getChunkLog(
        std::chrono::system_clock::time_point lastKnownTimestamp,
        GetChunkLogHandler handler) = 0;

    virtual void downloadChunk(
        const std::string& deviceId,
        int streamIndex,
        std::chrono::system_clock::time_point timestamp,
        DownloadHandler handler) = 0;

    //---------------------------------------------------------------------------------------------
    // Removing chunks.

    //virtual void removeAllChunksOlderThan(
    //    std::chrono::system_clock::time_point timestamp,
    //    Handler handler) = 0;

    virtual void removeChunk(
        const std::string& deviceId,
        int streamIndex,
        std::chrono::system_clock::time_point timestamp,
        Handler handler) = 0;

    //---------------------------------------------------------------------------------------------
    // Auxiliary.

    virtual void saveDeviceDescription(
        const std::string& cameraId,
        const DeviceDescription& data,
        Handler handler) = 0;

    virtual void getDeviceDescription(
        const std::string& cameraId,
        GetDeviceDescriptionHandler handler) = 0;
};

// TODO: #ak Upload a directory while monitoring for changes in it.

} // namespace nx::cloud::storage::client
