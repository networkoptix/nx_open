// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "stream_writer.h"

#include <nx/kit/debug.h>
#include <nx/sdk/result.h>

#include "stub_cloud_storage_plugin_ini.h"

namespace nx::vms_server_plugins::cloud_storage::stub {

using namespace std::chrono;

StreamWriter::StreamWriter(
    const std::shared_ptr<DataManager>& dataManager,
    const std::string& deviceId,
    int streamIndex,
    int64_t startTimeMs,
    const nx::sdk::IList<nx::sdk::cloud_storage::ICodecInfo>* codecList,
    const char* opaqueMetadata)
    :
    m_dataManager(dataManager)
{
    m_mediaFile = m_dataManager->writableMediaFile(
        deviceId, streamIndex, milliseconds(startTimeMs), codecList, opaqueMetadata);
}

nx::sdk::ErrorCode StreamWriter::putData(const nx::sdk::cloud_storage::IMediaDataPacket* packet)
{
    try
    {
        m_mediaFile->writeMediaPacket(packet);
        return nx::sdk::ErrorCode::noError;
    }
    catch (const std::exception& e)
    {
        NX_OUTPUT << "putData failed. Error: " << e.what();
        return nx::sdk::ErrorCode::ioError;
    }
}

nx::sdk::ErrorCode StreamWriter::close(int64_t durationMs)
{
    try
    {
        m_mediaFile->close(milliseconds(durationMs));
        return nx::sdk::ErrorCode::noError;
    }
    catch (const std::exception& e)
    {
        NX_OUTPUT << "close failed. Error: " << e.what();
        return nx::sdk::ErrorCode::ioError;
    }
}

int StreamWriter::size() const
{
    return m_mediaFile->size();
}

} // namespace nx::vms_server_plugins::cloud_storage::stub
