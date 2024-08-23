// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_agent.h"

#include <nx/kit/debug.h>
#include <nx/sdk/cloud_storage/helpers/data.h>
#include <nx/sdk/helpers/device_info.h>
#include <nx/sdk/helpers/string.h>

#include "stream_reader.h"
#include "stream_writer.h"
#include "stub_cloud_storage_plugin_ini.h"
#include "data_manager.h"

namespace nx::vms_server_plugins::cloud_storage::stub {

DeviceAgent::DeviceAgent(
    const nx::sdk::cloud_storage::DeviceDescription& deviceDescription,
    const std::shared_ptr<DataManager> metadataManager)
    :
    m_dataManager(metadataManager),
    m_deviceDescription(deviceDescription)
{
    m_dataManager->addDevice(deviceDescription);
}

void DeviceAgent::getDeviceInfo(nx::sdk::Result<const nx::sdk::IDeviceInfo*>* outResult) const
{
    *outResult = nx::sdk::Result<const nx::sdk::IDeviceInfo*>(
        nx::sdk::cloud_storage::deviceInfo(m_deviceDescription));
}

void DeviceAgent::doCreateStreamWriter(
    nxcip::MediaStreamQuality quality,
    int64_t startTimeMs,
    const nx::sdk::IList<nx::sdk::cloud_storage::ICodecInfo>* codecList,
    const char* opaqueMetadata,
    nx::sdk::Result<nx::sdk::cloud_storage::IStreamWriter*>* outResult)
{
    int streamIndex = nx::sdk::cloud_storage::toStreamIndex(quality);
    const auto deviceId = *m_deviceDescription.deviceId();
    try
    {
        *outResult = nx::sdk::Result<nx::sdk::cloud_storage::IStreamWriter*>(
            new StreamWriter(
                m_dataManager, deviceId, streamIndex, startTimeMs, codecList, opaqueMetadata));

        NX_OUTPUT << __func__ << ": Successfully created StreamWriter for device '"
            << deviceId << "', stream: " << streamIndex;
    }
    catch (const std::exception& e)
    {
        *outResult = nx::sdk::Result<nx::sdk::cloud_storage::IStreamWriter*>(
            nx::sdk::Error(nx::sdk::ErrorCode::internalError, new nx::sdk::String(e.what())));

        NX_OUTPUT << __func__ << ": Failed to create StreamWriter for device '" << deviceId
            << "', stream: " << streamIndex << ", error: '" << e.what() << "'";
    }
}

void DeviceAgent::doCreateStreamReader(
    nxcip::MediaStreamQuality quality,
    int64_t startTimeMs,
    int64_t durationMs,
    nx::sdk::Result<nx::sdk::cloud_storage::IStreamReader*>* outResult)
{
    int streamIndex = nx::sdk::cloud_storage::toStreamIndex(quality);
    const auto deviceId = *m_deviceDescription.deviceId();
    try
    {
        *outResult = nx::sdk::Result<nx::sdk::cloud_storage::IStreamReader*>(
            new StreamReader(m_dataManager, deviceId, streamIndex, startTimeMs, durationMs));
    }
    catch (const std::exception& e)
    {
        *outResult = nx::sdk::Result<nx::sdk::cloud_storage::IStreamReader*>(
            nx::sdk::Error(nx::sdk::ErrorCode::invalidParams, new nx::sdk::String(e.what())));

        NX_OUTPUT << __func__ << ": Failed to create ArchiveReader for device: '"
            << deviceId << "', stream index: " << streamIndex << ", timestamp: " << startTimeMs
            << ", error: " << e.what();
    }
}

} // namespace nx::vms_server_plugins::cloud_storage::stub
