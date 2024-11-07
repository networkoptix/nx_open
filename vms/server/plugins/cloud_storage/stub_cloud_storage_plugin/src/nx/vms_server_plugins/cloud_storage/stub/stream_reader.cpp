// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "stream_reader.h"

#include <nx/kit/debug.h>
#include <nx/sdk/cloud_storage/helpers/codec_info.h>
#include <nx/sdk/cloud_storage/helpers/media_data_packet.h>
#include <nx/sdk/cloud_storage/i_stream_reader.h>
#include <nx/sdk/ptr.h>
#include <nx/sdk/helpers/string.h>

#include "stub_cloud_storage_plugin_ini.h"

namespace nx::vms_server_plugins::cloud_storage::stub {

using namespace std::chrono;

StreamReader::StreamReader(
    const std::shared_ptr<DataManager>& dataManager,
    const std::string& deviceId,
    int streamIndex,
    int64_t startTimeMs,
    int64_t durationMs)
    :
    m_dataManager(dataManager),
    m_deviceId(deviceId),
    m_streamIndex(streamIndex),
    m_timestampUs(startTimeMs * 1000),
    m_durationUs(durationMs * 1000)
{
    m_file = m_dataManager->readableMediaFile(deviceId, streamIndex, startTimeMs, durationMs);
    NX_OUTPUT << "Opening StreamReader for " << m_file->name();
    m_codecInfos = nx::sdk::makePtr<nx::sdk::List<nx::sdk::cloud_storage::ICodecInfo>>();
    m_opaqueMetadata = m_file->opaqueMetadata();
    for (const auto& codecInfo: m_file->codecInfo())
    {
        m_codecInfos->addItem(
            nx::sdk::makePtr<nx::sdk::cloud_storage::CodecInfo>(codecInfo).get());
    }
}

void StreamReader::getOpaqueMetadata(nx::sdk::Result<const nx::sdk::IString*>* outResult) const
{
    *outResult = nx::sdk::Result<const nx::sdk::IString*>(new nx::sdk::String(m_opaqueMetadata));
}

const nx::sdk::IList<nx::sdk::cloud_storage::ICodecInfo>* StreamReader::getCodecInfoList() const
{
    m_codecInfos->addRef();
    return m_codecInfos.get();
}

int StreamReader::size() const
{
    return m_file ? m_file->size() : -1;
}

nx::sdk::ErrorCode StreamReader::getNextData(nx::sdk::cloud_storage::IMediaDataPacket** packet)
{
    try
    {
        auto data = m_file->getNextPacket();
        if (!data)
            return nx::sdk::ErrorCode::noData;

        if (++m_packetCount != 0 && m_packetCount % 100 == 0)
            NX_OUTPUT << "Successfully read " << m_packetCount << " from file " << m_file->name();

        *packet = new nx::sdk::cloud_storage::MediaDataPacket(*data);
        return nx::sdk::ErrorCode::noError;
    }
    catch (const std::exception&)
    {
        NX_OUTPUT << "Failed to read next packet from file " << m_file->name();
        return nx::sdk::ErrorCode::ioError;
    }
}

nx::sdk::ErrorCode StreamReader::seek(
    int64_t timestampUs,
    bool findKeyFrame,
    int64_t* selectedPositionUs)
{
    if ((int64_t) timestampUs > m_timestampUs + m_durationUs)
        return nx::sdk::ErrorCode::noData;

    try
    {
        m_file->seek(timestampUs, findKeyFrame, selectedPositionUs);
        return nx::sdk::ErrorCode::noError;
    }
    catch (const std::exception& e)
    {
        NX_OUTPUT << "Seek failed. timestamp: " << timestampUs << " file: " << m_file->name();
        return nx::sdk::ErrorCode::ioError;
    }
}

int64_t StreamReader::startTimeUs() const
{
    return m_timestampUs;
}

int64_t StreamReader::endTimeUs() const
{
    return m_timestampUs + m_durationUs;
}

} // namespace nx::vms_server_plugins::cloud_storage::stub
