// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_agent.h"

#include <nx/kit/debug.h>
#include <nx/sdk/cloud_storage/helpers/data.h>
#include <nx/sdk/helpers/device_info.h>
#include <nx/sdk/helpers/string.h>

#include "stream_reader.h"
#include "stream_writer.h"

namespace nx::vms_server_plugins::cloud_storage::sample {

DeviceAgent::DeviceAgent(
    const nx::sdk::cloud_storage::DeviceDescription& deviceDescription)
{
}

void DeviceAgent::getDeviceInfo(nx::sdk::Result<const nx::sdk::IDeviceInfo*>* outResult) const
{
    *outResult = nx::sdk::Result<const nx::sdk::IDeviceInfo*>(
        nx::sdk::Error(nx::sdk::ErrorCode::notImplemented, new nx::sdk::String("Not implemented")));
}

void DeviceAgent::doCreateStreamWriter(
    nxcip::MediaStreamQuality quality,
    int64_t startTimeMs,
    const nx::sdk::IList<nx::sdk::cloud_storage::ICodecInfo>* codecList,
    const char* opaqueMetadata,
    nx::sdk::Result<nx::sdk::cloud_storage::IStreamWriter*>* outResult)
{
    *outResult = nx::sdk::Result<nx::sdk::cloud_storage::IStreamWriter*>(
        nx::sdk::Error(nx::sdk::ErrorCode::notImplemented, new nx::sdk::String("Not implemented")));
}

void DeviceAgent::doCreateStreamReader(nxcip::MediaStreamQuality quality,
    int64_t startTimeMs,
    int64_t durationMs,
    nx::sdk::Result<nx::sdk::cloud_storage::IStreamReader*>* outResult)
{
    *outResult = nx::sdk::Result<nx::sdk::cloud_storage::IStreamReader*>(
        nx::sdk::Error(nx::sdk::ErrorCode::notImplemented, new nx::sdk::String("Not implemented")));
}

} // namespace nx::vms_server_plugins::cloud_storage::sample
