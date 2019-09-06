#pragma once

#include <chrono>
#include <string>
#include <vector>

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

NX_CLOUD_STORAGE_CLIENT_API std::string toString(const DeviceDescription&);

NX_CLOUD_STORAGE_CLIENT_API void fromString(
    const std::string_view& str,
    DeviceDescription* value);

} // namespace nx::cloud::storage::client
