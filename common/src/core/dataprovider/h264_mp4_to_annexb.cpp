////////////////////////////////////////////////////////////
// 20 feb 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "h264_mp4_to_annexb.h"


H264Mp4ToAnnexB::H264Mp4ToAnnexB( const AbstractOnDemandDataProviderPtr& dataSource )
:
    AbstractMediaDataFilter( dataSource ),
    m_isFirstPacket( true )
{
}

QnAbstractDataPacketPtr H264Mp4ToAnnexB::processData( QnAbstractDataPacketPtr* const data )
{
    static const quint8 START_CODE[] = { 0, 0, 0, 1 };
    QnAbstractMediaDataPtr srcMediaPacket = data->dynamicCast<QnAbstractMediaData>();
    if( !srcMediaPacket || srcMediaPacket->compressionType != CODEC_ID_H264 )
        return *data;
    if( srcMediaPacket->data.data()[3] == 1 )
        return *data;   //already in annexb format. TODO #ak: format validation is too weak

    //TODO #ak: copying packet
    QnAbstractMediaDataPtr mediaPacket = srcMediaPacket;

    //replacing NALU size with {0 0 0 1}
    for( quint8* dataStart = (quint8*)mediaPacket->data.data();
        dataStart < (quint8*)mediaPacket->data.data() + mediaPacket->data.size();
         )
    {
        size_t naluSize = ntohl( *(u_long*)dataStart );
        memcpy( dataStart, START_CODE, sizeof(START_CODE) );
        dataStart += sizeof(START_CODE) + naluSize;
    }

    if( m_isFirstPacket && isH264SeqHeaderInExtraData( mediaPacket ) )
    {
        m_newContext = QnMediaContextPtr( new QnMediaContext(mediaPacket->context->ctx()) );

        //reading sequence header from extradata
        std::basic_string<quint8> seqHeader;
        readH264SeqHeaderFromExtraData( mediaPacket, &seqHeader );
        if( seqHeader.size() > 0 )
        {
            QnByteArray mediaDataWithSeqHeader( FF_INPUT_BUFFER_PADDING_SIZE, seqHeader.size() + mediaPacket->data.size() );
            mediaDataWithSeqHeader.resize( seqHeader.size() + mediaPacket->data.size() );
            memcpy( mediaDataWithSeqHeader.data(), seqHeader.data(), seqHeader.size() );
            memcpy( mediaDataWithSeqHeader.data() + seqHeader.size(), mediaPacket->data.data(), mediaPacket->data.size() );
            mediaPacket->data = mediaDataWithSeqHeader;
            m_isFirstPacket = false;

            //TODO #ak: monitor sequence header change
        }

        m_newContext->ctx()->extradata_size = 0;
    }

    mediaPacket->context = m_newContext;

    return mediaPacket;
}

//dishonorably stolen from libavcodec source
#ifndef AV_RB16
#   define AV_RB16(x)                           \
    ((((const uint8_t*)(x))[0] << 8) |          \
      ((const uint8_t*)(x))[1])
#endif
static const quint8 H264_START_CODE[] = { 0, 0, 0, 1 };

bool H264Mp4ToAnnexB::isH264SeqHeaderInExtraData( const QnAbstractMediaDataPtr data ) const
{
    return data->context && data->context->ctx() && data->context->ctx()->extradata_size >= 7 && data->context->ctx()->extradata[0] == 1;
}

void H264Mp4ToAnnexB::readH264SeqHeaderFromExtraData(
    const QnAbstractMediaDataPtr data,
    std::basic_string<quint8>* const seqHeader )
{
    const unsigned char* p = data->context->ctx()->extradata;

    //sps & pps is in the extradata, parsing it...
    //following code has been taken from libavcodec/h264.c

    // prefix is unit len
    //const int reqUnitSize = (data->context->ctx()->extradata[4] & 0x03) + 1;
    /* sps and pps in the avcC always have length coded with 2 bytes,
     * so put a fake nal_length_size = 2 while parsing them */
    //int nal_length_size = 2;

    // Decode sps from avcC
    int cnt = *(p + 5) & 0x1f; // Number of sps
    p += 6;

    for( int i = 0; i < cnt; i++ )
    {
        const int nalsize = AV_RB16(p);
        p += 2; //skipping nalusize
        if( nalsize > data->context->ctx()->extradata_size - (p-data->context->ctx()->extradata) )
            break;
        seqHeader->append( H264_START_CODE, sizeof(H264_START_CODE) );
        seqHeader->append( p, nalsize );
        p += nalsize;
    }

    // Decode pps from avcC
    cnt = *(p++); // Number of pps
    for( int i = 0; i < cnt; ++i )
    {
        const int nalsize = AV_RB16(p);
        p += 2;
        if( nalsize > data->context->ctx()->extradata_size - (p-data->context->ctx()->extradata) )
            break;

        seqHeader->append( H264_START_CODE, sizeof(H264_START_CODE) );
        seqHeader->append( p, nalsize );
        p += nalsize;
    }
}
