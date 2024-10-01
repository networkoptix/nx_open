// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/cloud_storage/i_codec_info.h>
#include <nx/sdk/cloud_storage/i_stream_reader.h>
#include <nx/sdk/cloud_storage/i_stream_writer.h>
#include <nx/sdk/i_device_info.h>
#include <nx/sdk/i_list.h>
#include <nx/sdk/i_integration.h>
#include <nx/sdk/i_string.h>
#include <nx/sdk/interface.h>
#include <nx/sdk/result.h>

namespace nx::sdk::cloud_storage {

/**
 * An abstraction of the device agent. Used for obtaining of the stream writer and archive reader
 * objects in the context of the given device.
 */
class IDeviceAgent: public Interface<IDeviceAgent>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::archive::IDevice"); }

    /** Provides device description */
    protected: virtual void getDeviceInfo(Result<const IDeviceInfo*>* outResult) const = 0;
    public: virtual Result<const IDeviceInfo*> deviceInfo() const
    {
        Result<const IDeviceInfo*> result;
        getDeviceInfo(&result);
        return result;
    }

    protected: virtual void doCreateStreamWriter(
        nxcip::MediaStreamQuality quality,
        int64_t startTimeUs,
        const IList<ICodecInfo>* codecInfoList,
        const char* opaqueMetadata,
        Result<IStreamWriter*>* outResult) = 0;

    /**
     * Obtain a StreamWriter object. It is used for media stream (a sequence of media packets)
     * recording. Media stream may consist of several substreams (audio, video, metadata).
     * Each of substream is described via codec info object. Those stream attributes should be
     * stored by the plugin and returned back via the corresponding StreamReader object for the
     * given chunk of media data.
     * @param codecInfoList Codec description list, one per substream.
     * @param opaqueMetadata Arbitrary null terminated string that should be returned unchanged by
     * StreamReader::opaqueMetadata() for the corresponding chunk of media data.
     */
    public: Result<IStreamWriter*> createStreamWriter(
        nxcip::MediaStreamQuality quality,
        int64_t startTimeMs,
        const IList<ICodecInfo>* codecInfoList,
        const char* opaqueMetadata)
    {
        Result<IStreamWriter*> result;
        doCreateStreamWriter(quality, startTimeMs, codecInfoList, opaqueMetadata, &result);
        return result;
    }

    protected: virtual void doCreateStreamReader(
        nxcip::MediaStreamQuality quality,
        int64_t startTimeUs,
        int64_t durationUs,
        Result<IStreamReader*>* outResult) = 0;

    /**
    * Creates a StreamWriter object.
    * @param codecInfoList Codec description list, one per stream.
    * @param opaqueMetadata Arbitrary null terminated string that should be returned unchanged
    * by StreamReader::opaqueMetadata() for the corresponding chunk of media data.
    */
    public: Result<IStreamReader*> createStreamReader(
        nxcip::MediaStreamQuality quality,
        int64_t startTimeMs,
        int64_t durationMs)
    {
            Result<IStreamReader*> result;
        doCreateStreamReader(quality, startTimeMs, durationMs, &result);
        return result;
    }
};

} // namespace nx::sdk::cloud_storage
