#include <chrono>
#include <optional>
#include <string>

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/buffer.h>
#include <nx/utils/std/filesystem.h>
#include <nx/utils/url.h>

namespace nx::cloud::storage::client {

enum class ResultCode
{
    ok = 0,
    ioError,
};

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

    /**
     * @param writerId Id of storage client. Should be unique for every client and
     *   preserved for every storage usage by the same client.
     * MUST be invoked prior to any other method.
     * TODO: #ak Authentication.
     */
    virtual void open(
        const nx::utils::Url& url,
        const std::string& storageClientId,
        Handler handler) = 0;

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

//-------------------------------------------------------------------------------------------------

struct MetadataRecord
{
    // TODO
};

class AbstractMetadataClient:
    public nx::network::aio::BasicPollable
{
public:
    using OpenHandler = nx::utils::MoveOnlyFunc<void(ResultCode)>;
    using ErrorHandler = nx::utils::MoveOnlyFunc<void(ResultCode)>;

    virtual void open(OpenHandler handler) = 0;

    /**
     * The data can be buffered inside and uploaded with some delay.
     * If an error happens while upoading, then upload is retried.
     * NOTE: Can be invoked only after successful AbstractMetadataClient::open completion.
     */
    virtual void save(MetadataRecord data) = 0;

    /**
     * @param handler Invoked on some error.
     * NOTE: An error does not stop upload attempts.
     */
    virtual void setOnError(ErrorHandler handler) = 0;

    // TODO
};

} // namespace nx::cloud::storage::client
