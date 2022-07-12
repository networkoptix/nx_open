// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

////////////////////////////////////////////////////////////
// 20 feb 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////
#pragma once

#include "abstract_media_data_filter.h"
#include <nx/streaming/media_data_packet.h>
#include <nx/streaming/video_data_packet.h>

//!Converts h.264/h.265 stream from avc file format (mpeg4 part 15) to mpeg 4 annex b
/*!
    Takes sequence header from media packet's extradata and inserts it before first packet.
    If source stream is not h.264/h.265 or is already in Annex B format, this class just forwards
    data from source to the reader
*/
class NX_VMS_COMMON_API H2645Mp4ToAnnexB: public AbstractMediaDataFilter
{
public:
    //!Implementation of AbstractMediaDataFilter::processData
    virtual QnConstAbstractDataPacketPtr processData(
        const QnConstAbstractDataPacketPtr& data) override;

    // Same as processData, but return QnCompressedVideoDataPtr.
    QnConstCompressedVideoDataPtr processVideoData(const QnConstCompressedVideoDataPtr& data);

private:
    CodecParametersConstPtr m_newContext;
};
