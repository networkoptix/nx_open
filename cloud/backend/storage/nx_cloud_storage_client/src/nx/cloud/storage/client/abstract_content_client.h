#pragma once

#include <chrono>
#include <string>
#include <vector>

#include <nx/network/aio/basic_pollable.h>
#include <nx/utils/move_only_func.h>

#include "result_code.h"

namespace nx::cloud::storage::client {

enum class ChunkOperation
{
    none = 0,
    add,
    remove,
};

struct ChunkLogEntry
{
    ChunkOperation action = ChunkOperation::none;
    std::string deviceId;
    std::chrono::system_clock::time_point timestamp;

    /**
     * Stream index used when uploading chunks.
     * E.g., 0 - high-quality stream, 1 - low-quality stream.
     */
    int streamIndex = 0;

    /** Specified only for ChunkOperation::add. */
    std::size_t size = 0;
};

using DeviceDescription = std::vector<std::pair<std::string, std::string>>;

/**
 * NOTE: Chunk log is modified automatically when uploading/removing chunks.
 */
class AbstractContentClient:
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

    virtual void uploadMediaChunk(
        const std::string& deviceId,
        int streamIndex,
        std::chrono::system_clock::time_point timestamp,
        const nx::Buffer& data,
        Handler handler) = 0;

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
