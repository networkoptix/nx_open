// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/interface.h>
#include <nx/sdk/i_plugin.h>
#include <nx/sdk/result.h>
#include <camera/camera_plugin.h>

namespace nx {
namespace sdk {
namespace archive {

/**
 * When Mediaserver is finished with the current stream ~IStreamWriter() will be called.
 * Implementation might do some finalization at this point.
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
    virtual ErrorCode putData(const nxcip::MediaDataPacket* packet) = 0;

    /**
     * This function should be called just before destruction of the StreamWriter object and no
     * other calls should follow.
     */
    virtual void close(int64_t durationMs) = 0;
};

} // namespace archive
} // namespace sdk
} // namespace nx
