// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/interface.h>
#include <nx/sdk/i_device_info.h>
#include <nx/sdk/i_plugin.h>
#include <nx/sdk/result.h>
#include <nx/sdk/i_list.h>
#include <nx/sdk/i_string.h>
#include <camera/camera_plugin.h>

#include <nx/sdk/archive/i_stream_writer.h>
#include <nx/sdk/archive/i_codec_info.h>

namespace nx::sdk {
namespace archive {

enum class MetadataType
{
    motion,
    analytics,
    bookmark,
};

/**
 * An archive streaming/recording device. Recording is done via StreamWriter. Archive information
 * and data stream is provided by the nxcip::DtsArchiveReader class.
 */
class IDevice: public Interface<IDevice>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::archive::IDevice"); }

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
        const char* opaqueMetadata,
        Result<IStreamWriter*>* outResult) = 0;

    /**
     * Creates a StreamWriter object.
     * @param codecList Codec description list, one per stream.
     * @param opaqueMetadata Arbitrary null terminated string that should be returned unchanged by
     * StreamReader2::opaqueMetadata() for the corresponding chunk of media data.
     */
    public: Result<IStreamWriter*> createStreamWriter(
        nxcip::MediaStreamQuality quality,
        int64_t startTimeUs,
        const IList<ICodecInfo>* codecList,
        const char* opaqueMetadata)
    {
        Result<IStreamWriter*> result;
        doCreateStreamWriter(quality, startTimeUs, codecList, opaqueMetadata, &result);
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

    /**
    * @param data Null terminated string containing JSON of the corresponding metadata object.
    * JSON formats are the following:
    * Motion
    * TODO
    * Analytics
    * TODO
    * Bookmark
    * TODO
    */
    public: virtual ErrorCode saveMetadata(
        int64_t timeStampUs,
        MetadataType type,
        const char* data) = 0;
};

} // namespace archive
} // namespace nx::sdk
