// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "stream_reader.h"

#include <nx/kit/debug.h>
#include <nx/sdk/cloud_storage/helpers/codec_info.h>
#include <nx/sdk/cloud_storage/helpers/media_data_packet.h>
#include <nx/sdk/cloud_storage/i_codec_info.h>
#include <nx/sdk/cloud_storage/i_stream_reader.h>
#include <nx/sdk/helpers/list.h>
#include <nx/sdk/helpers/string.h>
#include <nx/sdk/ptr.h>

namespace nx::vms_server_plugins::cloud_storage::sample {

using namespace std::chrono;

StreamReader::StreamReader(
    const std::string& /*deviceId*/,
    int /*streamIndex*/,
    int64_t /*startTimeMs*/,
    int64_t /*durationMs*/)
{
}

const nx::sdk::IList<nx::sdk::cloud_storage::ICodecInfo>* StreamReader::getCodecInfoList() const
{
    return nullptr;
}

void StreamReader::getOpaqueMetadata(
    nx::sdk::Result<const nx::sdk::IString*>* outResult) const
{
    *outResult = nx::sdk::Result<const nx::sdk::IString*>(nx::sdk::Error(
        nx::sdk::ErrorCode::notImplemented, new nx::sdk::String("Not implemented")));
}

nx::sdk::ErrorCode StreamReader::getNextData(
    nx::sdk::cloud_storage::IMediaDataPacket** /*packet*/)
{
    return nx::sdk::ErrorCode::noData;
}

nx::sdk::ErrorCode StreamReader::seek(
    int64_t /*timestampUs*/, bool /*findKeyFrame*/, int64_t* /*selectedPositionUs*/)
{
    return nx::sdk::ErrorCode::notImplemented;
}

int64_t StreamReader::startTimeUs() const
{
    return -1;
}

int64_t StreamReader::endTimeUs() const
{
    return -1;
}

} // namespace nx::vms_server_plugins::cloud_storage::sample
