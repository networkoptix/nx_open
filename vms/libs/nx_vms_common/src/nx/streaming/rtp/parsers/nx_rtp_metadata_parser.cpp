// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "nx_rtp_metadata_parser.h"

namespace nx::rtp {

Result NxRtpMetadataParser::processData(
    const RtpHeader& /*rtpHeader*/,
    quint8* /*rtpBufferBase*/,
    int /*bufferOffset*/,
    int /*bytesRead*/,
    bool& gotData)
{
    // RTSP metadata stream is not used so far.
    gotData = false;
    return Result();
}

QnAbstractMediaDataPtr NxRtpMetadataParser::nextData()
{
    // RTSP metadata stream is not used so far.
    return QnAbstractMediaDataPtr();
}

} // namespace nx::rtp
