// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/cloud_storage/i_stream_reader.h>
#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/result.h>

namespace nx::vms_server_plugins::cloud_storage::sample {

class StreamReader: nx::sdk::RefCountable<nx::sdk::cloud_storage::IStreamReader>
{
public:
    StreamReader(
        const std::string& deviceId,
        int streamIndex,
        int64_t startTimeMs,
        int64_t durationMs);

    virtual nx::sdk::ErrorCode getNextData(
        nx::sdk::cloud_storage::IMediaDataPacket** packet) override;

    virtual int64_t startTimeUs() const override;
    virtual int64_t endTimeUs() const override;

    virtual nx::sdk::ErrorCode seek(
        int64_t timestampUs, bool findKeyFrame, int64_t* selectedPositionUs) override;

protected:
    virtual void getOpaqueMetadata(
        nx::sdk::Result<const nx::sdk::IString*>* outResult) const override;
    virtual const nx::sdk::IList<nx::sdk::cloud_storage::ICodecInfo>* getCodecInfoList() const override;
};

} // namespace nx::vms_server_plugins::cloud_storage::sample
