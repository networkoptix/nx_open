// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <camera/camera_plugin.h>
#include <nx/sdk/i_integration.h>
#include <nx/sdk/interface.h>
#include <nx/sdk/result.h>

#include "i_media_data_packet.h"

namespace nx::sdk::cloud_storage {

/**
 * Stream writer abstraction. Stream is a sequence of media data packets, which more or
 * less represent continuous chunk of media and metadata for the given period of time.
 * When Mediaserver is finished with the current stream ~IStreamWriter() will be called.
 * Implementation might do some finalization at this point.
 * Server will create a StreamWriter object for each chunk of media data. So the media stream,
 * being continuous is still cut on the small chunks of data each of which has its timestamp
 * and duration. Each of those chunks are recorded using Stream Writer and subsequently
 * requested back using a corresponding StreamReader object.
 */
class IStreamWriter: public Interface<IStreamWriter>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::archive::IStreamWriter"); }

    /**
     * Write a data packet. Implementation is discouraged to buffer packets. Instead, it should
     * block until the data is completely processed.
     *
     * packet->channelNumber() corresponds to ICodecInfo::channelNumber() i.e. if packet->type() ==
     * dptVideo && packet->channelNumber() == 1, then CodecInfo with
     * mediaType == AVMEDIA_TYPE_VIDEO && channelNumber == 1 can be used to process this packet.
     *
     * If packet->type() == dptAudio && packet->channelNumber() == 0, then CodecInfo with
     * mediaType == AVMEDIA_TYPE_AUDIO && channelNumber == 0 can be used to process this packet.
     *
     * If packet->type() == dptData && packet->channelNumber() == 0, then CodecInfo with
     * mediaType == AVMEDIA_TYPE_DATA && channelNumber == 0 can be used to process this packet.
     */
    virtual ErrorCode putData(const IMediaDataPacket* packet) = 0;

    /**
     * This function will be called just before destruction of the StreamWriter object and no
     * other calls (except destructor) will follow.
     */
    virtual ErrorCode close(int64_t durationMs) = 0;

    /**
     * Total size of written data so far.
     */
    virtual int size() const = 0;
};

} // namespace nx::sdk::cloud_storage
