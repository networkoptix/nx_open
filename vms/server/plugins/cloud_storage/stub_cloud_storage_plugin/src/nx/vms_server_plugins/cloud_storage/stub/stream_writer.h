// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/cloud_storage/i_codec_info.h>
#include <nx/sdk/cloud_storage/i_stream_writer.h>
#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/i_list.h>
#include <nx/sdk/result.h>

#include "data_manager.h"

namespace nx::vms_server_plugins::cloud_storage::stub {

class StreamWriter: public nx::sdk::RefCountable<nx::sdk::cloud_storage::IStreamWriter>
{
public:
    StreamWriter(
        const std::shared_ptr<DataManager>& dataManager,
        const std::string& deviceId,
        int streamIndex,
        int64_t startTimeMs,
        const nx::sdk::IList<nx::sdk::cloud_storage::ICodecInfo>* codecList,
        const char* opaqueMetadata);

    virtual nx::sdk::ErrorCode putData(const nx::sdk::cloud_storage::IMediaDataPacket* packet) override;
    virtual nx::sdk::ErrorCode close(int64_t durationMs) override;
    virtual int size() const override;

private:
    std::shared_ptr<DataManager> m_dataManager;
    std::unique_ptr<WritableMediaFile> m_mediaFile;
};

} // namespace nx::vms_server_plugins::cloud_storage::stub
