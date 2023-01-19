// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <stdint.h>

#include <nx/sdk/interface.h>

#include "i_data_packet.h"
#include "i_media_context.h"

namespace nx::sdk::analytics {

/**
 * Packet containing compressed media data (audio or video).
 */
class ICompressedMediaPacket: public Interface<ICompressedMediaPacket, IDataPacket0>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::analytics::ICompressedMediaPacket"); }

    /**
     * @return Null-terminated ASCII string containing the MIME type corresponding to the packet
     * data compression.
     * For the video data, the following MIME types are available:
     * - "video/h264" for H264 streams
     * - "video/hevc" for H265/HEVC streams
     * - "video/mpeg4" for MPEG-4 streams
     * - "video/mjpeg" for MJPEG streams
     *
     * For the metadata, check the [Onvif documentation](https://www.onvif.org/specs/stream/ONVIF-Streaming-Spec-v210.pdf).
     */
    virtual const char* codec() const = 0;

    /**
     * @return Pointer to the compressed data.
     */
    virtual const char* data() const = 0;

    /**
     * @return Size of the compressed data in bytes.
     */
    virtual int dataSize() const = 0;

    /** Called by context() */
    protected: virtual const IMediaContext* getContext() const = 0;
    /**
     * @return Pointer to the codec context, or null if not available.
     */
    public: Ptr<const IMediaContext> context() const { return Ptr(getContext()); }

    enum class MediaFlags: uint32_t
    {
        none = 0,
        keyFrame = 1 << 0,
        all = UINT32_MAX
    };

    /**
     * @return Packet media flags.
     */
    virtual MediaFlags flags() const = 0;
};
using ICompressedMediaPacket0 = ICompressedMediaPacket;

} // namespace nx::sdk::analytics
