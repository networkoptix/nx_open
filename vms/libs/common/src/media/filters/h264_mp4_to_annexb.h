////////////////////////////////////////////////////////////
// 20 feb 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////
#pragma once

#include "abstract_media_data_filter.h"
#include <nx/streaming/media_data_packet.h>

//!Converts h.264/h.265 stream from avc file format (mpeg4 part 15) to mpeg 4 annex b
/*!
    Takes sequence header from media packet's extradata and inserts it before first packet.
    If source stream is not h.264/h.265 or is already in Annex B format, this class just forwards
    data from source to the reader
*/
class H2645Mp4ToAnnexB: public AbstractMediaDataFilter
{
public:
    //!Implementation of AbstractMediaDataFilter::processData
    virtual QnConstAbstractDataPacketPtr processData(const QnConstAbstractDataPacketPtr& data ) override;

private:
    QnConstMediaContextPtr m_newContext;
};
