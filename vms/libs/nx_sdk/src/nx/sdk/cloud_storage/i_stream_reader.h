// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <camera/camera_plugin.h>
#include <nx/sdk/cloud_storage/i_codec_info.h>
#include <nx/sdk/cloud_storage/i_media_data_packet.h>
#include <nx/sdk/i_list.h>
#include <nx/sdk/interface.h>

namespace nx::sdk::cloud_storage {

/**
 * Stream Reader for the media data chunk previously recorded by the Stream Writer.
 */
class IStreamReader: public Interface<IStreamReader>
{
public:
    static constexpr auto interfaceId() { return makeId("nx::sdk::archive::StreamReader"); }

    /**
     * Attempts to read a next media data packet. Should return ErrorCode::noData if there is
     * no data left to read.
     */
    public: virtual ErrorCode getNextData(IMediaDataPacket** packet) = 0;

    protected: virtual void getOpaqueMetadata(Result<const IString*>* outResult) const = 0;
    /**
    * Returns opaque metadata that was provided to the plugin when a stream writer for the
    * corresponding media data chunk was created.
    */
    public: Result<const IString*> opaqueMetadata() const
    {
        Result<const IString*> result;
        getOpaqueMetadata(&result);
        return result;
    }

    protected: virtual const IList<ICodecInfo>* getCodecInfoList() const = 0;
    /**
    * Returns codec info list that was provided to the plugin when a stream writer for the
    * corresponding media data chunk was created.
    */
    public: Ptr<const IList<ICodecInfo>> codecInfoList() const { return Ptr(getCodecInfoList()); }

    virtual int64_t startTimeUs() const = 0;
    virtual int64_t endTimeUs() const = 0;

    /**
     * Moves stream data cursor to the required position if possible. This position may be
     * greater than requested if there is no media data packet with the exact timestamp.
     * If 'findKeyFrame' is true, selected position should correspond to the media data packet
     * which has 'isKeyFrame' flag set.
     */
    virtual ErrorCode seek(int64_t timestampUs, bool findKeyFrame, int64_t* selectedPositionUs) = 0;
};

} // namespace nx::sdk::cloud_storage
