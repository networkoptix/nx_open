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

//-------------------------------------------------------------------------------------------------
// DeviceDescription.

struct Parameter
{
    std::string name;
    std::string value;
};

inline bool operator==(const Parameter& one, const Parameter& two)
{
    return one.name == two.name && one.value == two.value;
}

using DeviceDescription = std::vector<Parameter>;

NX_CLOUD_STORAGE_CLIENT_API std::string toString(const DeviceDescription&);

NX_CLOUD_STORAGE_CLIENT_API void fromString(
    const std::string_view& str,
    DeviceDescription* value);

template <typename T> T fromString(const std::string_view& str) = delete;

template<>
inline DeviceDescription fromString<DeviceDescription>(const std::string_view& str)
{
    DeviceDescription description;
    fromString(str, &description);
    return description;
}

} // namespace nx::cloud::storage::client
