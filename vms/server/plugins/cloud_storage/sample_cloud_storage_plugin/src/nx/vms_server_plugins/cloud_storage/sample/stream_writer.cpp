// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "stream_writer.h"

#include <nx/kit/debug.h>

namespace nx::vms_server_plugins::cloud_storage::sample {

using namespace std::chrono;

StreamWriter::StreamWriter(
    const std::string& /*deviceId*/,
    int /*streamIndex*/,
    int64_t /*startTimeMs*/,
    const nx::sdk::IList<nx::sdk::cloud_storage::ICodecInfo>* /*codecList*/,
    const char* /*opaqueMetadata*/)
{
}

nx::sdk::ErrorCode StreamWriter::putData(const nx::sdk::cloud_storage::IMediaDataPacket* /*packet*/)
{
    return nx::sdk::ErrorCode::notImplemented;
}

nx::sdk::ErrorCode StreamWriter::close(int64_t /*durationMs*/)
{
    return nx::sdk::ErrorCode::notImplemented;
}

int StreamWriter::size() const
{
    return -1;
}

} // namespace nx::vms_server_plugins::cloud_storage::sample
