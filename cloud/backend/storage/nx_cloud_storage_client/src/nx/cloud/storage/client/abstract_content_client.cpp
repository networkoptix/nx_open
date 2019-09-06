#include "abstract_content_client.h"

namespace nx::cloud::storage::client {

void AbstractContentClient::uploadMediaChunk(
    const std::string& /*deviceId*/,
    int /*streamIndex*/,
    std::chrono::system_clock::time_point /*timestamp*/,
    const nx::Buffer& /*data*/,
    Handler handler)
{
    // TODO
    handler(ResultCode::notImplemented);
}

} // namespace nx::cloud::storage::client
