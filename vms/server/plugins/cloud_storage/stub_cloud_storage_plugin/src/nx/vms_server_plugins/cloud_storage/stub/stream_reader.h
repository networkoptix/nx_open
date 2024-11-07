// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

#include <camera/camera_plugin.h>
#include <nx/sdk/cloud_storage/i_codec_info.h>
#include <nx/sdk/cloud_storage/i_stream_reader.h>
#include <nx/sdk/helpers/list.h>
#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/result.h>
#include <plugins/plugin_tools.h>

#include "data_manager.h"

namespace nx::vms_server_plugins::cloud_storage::stub {

class StreamReader: public nx::sdk::RefCountable<nx::sdk::cloud_storage::IStreamReader>
{
public:
    StreamReader(
        const std::shared_ptr<DataManager>& dataManager,
        const std::string& deviceId,
        int streamIndex,
        int64_t startTimeMs,
        int64_t durationMs);

    virtual nx::sdk::ErrorCode getNextData(nx::sdk::cloud_storage::IMediaDataPacket** packet) override;
    virtual int64_t startTimeUs() const override;
    virtual int64_t endTimeUs() const override;
    virtual nx::sdk::ErrorCode seek(
        int64_t timestampUs, bool findKeyFrame, int64_t* selectedPositionUs) override;
    virtual int size() const override;

protected:
    virtual void getOpaqueMetadata(
        nx::sdk::Result<const nx::sdk::IString*>* outResult) const override;
    virtual const nx::sdk::IList<nx::sdk::cloud_storage::ICodecInfo>* getCodecInfoList() const override;

private:
    std::shared_ptr<DataManager> m_dataManager;
    std::string m_deviceId;
    int m_streamIndex = -1;
    const int64_t m_timestampUs;
    const int64_t m_durationUs;
    nx::sdk::Ptr<nx::sdk::List<nx::sdk::cloud_storage::ICodecInfo>> m_codecInfos;
    std::unique_ptr<ReadableMediaFile> m_file;
    std::string m_opaqueMetadata;
    int m_packetCount = 0;

};

} // namespace nx::vms_server_plugins::cloud_storage::stub
