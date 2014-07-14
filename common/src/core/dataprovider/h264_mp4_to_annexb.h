////////////////////////////////////////////////////////////
// 20 feb 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef H264_MP4_TO_ANNEXB_H
#define H264_MP4_TO_ANNEXB_H

#ifdef ENABLE_DATA_PROVIDERS

#include "abstract_media_data_filter.h"

#include "../datapacket/media_data_packet.h"


//!Converts h.264 stream from avc file format (mpeg4 part 15) to mpeg 4 annex b
/*!
    Takes sequence header from media packet's extradata and inserts it before first packet.
    
    If source stream is not h.264 or is already in Annex B format, this class just forwards data from source to the reader
*/
class H264Mp4ToAnnexB
:
    public AbstractMediaDataFilter
{
public:
    H264Mp4ToAnnexB( const AbstractOnDemandDataProviderPtr& dataSource );

protected:
    //!Implementation of AbstractMediaDataFilter::processData
    virtual QnAbstractDataPacketPtr processData( QnAbstractDataPacketPtr* const data ) override;

private:
    bool m_isFirstPacket;
    QnMediaContextPtr m_newContext;

    void readH264SeqHeaderFromExtraData(
        const QnAbstractMediaDataPtr& data,
        std::basic_string<quint8>* const seqHeader );
    bool isH264SeqHeaderInExtraData( const QnAbstractMediaDataPtr& data ) const;
};

#endif // ENABLE_DATA_PROVIDERS

#endif  //H264_MP4_TO_ANNEXB_H
