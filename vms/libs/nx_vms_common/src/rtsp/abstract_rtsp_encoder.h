// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "nx/streaming/media_data_packet.h"
#include "utils/common/byte_array.h"

/*
 * Base class for RTSP codec. Used for encode data only
 */

class NX_VMS_COMMON_API AbstractRtspEncoder
{
public:
    virtual ~AbstractRtspEncoder() {}

    virtual QString getSdpMedia(bool isVideo, int trackId, int port = 0) = 0;

    /*
     * Set media packet to encode
     */
    virtual void setDataPacket(QnConstAbstractMediaDataPtr media) = 0;

    /*
     * Function MUST write encoded data packet to sendBuffer. sendBuffer may contain some data
     * before function call (TCP RTSP header). So, function MUST add data only without clearing
     * buffer return true if some data are writed or false if no more data.
     */
    virtual bool getNextPacket(QnByteArray& sendBuffer) = 0;

    virtual void init() = 0;
    virtual bool isEof() const = 0;
};

using AbstractRtspEncoderPtr = std::shared_ptr<AbstractRtspEncoder>;
