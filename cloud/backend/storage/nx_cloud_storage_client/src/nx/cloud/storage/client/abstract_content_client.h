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
    add = 0,
    remove,
};

struct ChunkLogEntry
{
    ChunkOperation action;
    std::string deviceId;
    std::chrono::system_clock::time_point timestamp;
};

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

    //---------------------------------------------------------------------------------------------
    // Uploading chunks.

    virtual void uploadMediaChunk(
        const std::string& deviceId,
        std::chrono::system_clock::time_point timestamp,
        const std::filesystem::path& sourcePath,
        Handler handler) = 0;

    virtual void uploadMediaChunk(
        const std::string& deviceId,
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
        std::chrono::system_clock::time_point timestamp,
        DownloadHandler handler) = 0;

    //---------------------------------------------------------------------------------------------
    // Removing chunks.

    //virtual void removeAllChunksOlderThan(
    //    std::chrono::system_clock::time_point timestamp,
    //    Handler handler) = 0;

    virtual void removeChunk(
        const std::string& deviceId,
        std::chrono::system_clock::time_point timestamp,
        Handler handler) = 0;

    //---------------------------------------------------------------------------------------------
    // Auxiliary.

    virtual void uploadFile(
        const std::string& destinationPath,
        const nx::Buffer& data,
        Handler handler) = 0;

    virtual void downloadFile(
        const std::string& path,
        DownloadHandler handler) = 0;
};

// TODO: #ak Upload a directory while monitoring for changes in it.

} // namespace nx::cloud::storage::client
