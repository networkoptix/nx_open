// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/interface.h>
#include <nx/sdk/i_device_info.h>
#include <nx/sdk/i_plugin.h>
#include <nx/sdk/result.h>
#include <nx/sdk/i_list.h>
#include <camera/camera_plugin.h>

#include <nx/sdk/archive/i_stream_writer.h>
#include <nx/sdk/archive/i_codec_info.h>

namespace nx {
namespace sdk {
namespace archive {

/**
 * An archive streaming/recording device. Recording is done via StreamWriter. Archive information
 * and data stream is provided by the nxcip::DtsArchiveReader class.
 */
class IDevice: public Interface<IDevice>
{
public:
    static constexpr auto interfaceId() { return makeId("nx::sdk::archive::IDevice"); }

    /** Provides device description */
    protected: virtual void doDeviceInfo(Result<const IDeviceInfo*>* outResult) const = 0;
    public: virtual Result<const IDeviceInfo*> deviceInfo() const
    {
        Result<const IDeviceInfo*> result;
        doDeviceInfo(&result);
        return result;
    }

    protected: virtual void doCreateStreamWriter(
        nxcip::MediaStreamQuality quality,
        int64_t startTimeUs,
        const IList<ICodecInfo>* codecList,
        Result<IStreamWriter*>* outResult) = 0;

    /**
     * @param codecList Codec description list, one per stream.
     * ArchiveReader later.
     */
    public: Result<IStreamWriter*> createStreamWriter(
        nxcip::MediaStreamQuality quality,
        int64_t startTimeUs,
        const IList<ICodecInfo>* codecList)
    {
        Result<IStreamWriter*> result;
        doCreateStreamWriter(quality, startTimeUs, codecList, &result);
        return result;
    }

    protected: virtual void doCreateArchiveReader(
        nxcip::MediaStreamQuality quality,
        int64_t startTimeUs,
        int64_t durationUs,
        Result<nxcip::DtsArchiveReader*>* outResult) = 0;

    public: Result<nxcip::DtsArchiveReader*> createArchiveReader(
        nxcip::MediaStreamQuality quality,
        int64_t startTimeUs,
        int64_t durationUs)
    {
        Result<nxcip::DtsArchiveReader*> result;
        doCreateArchiveReader(quality, startTimeUs, durationUs, &result);
        return result;
    }
};

} // namespace archive
} // namespace sdk
} // namespace nx
