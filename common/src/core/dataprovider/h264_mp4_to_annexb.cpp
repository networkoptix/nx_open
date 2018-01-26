////////////////////////////////////////////////////////////
// 20 feb 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "h264_mp4_to_annexb.h"

#ifdef ENABLE_DATA_PROVIDERS

#include "nx/streaming/video_data_packet.h"


H264Mp4ToAnnexB::H264Mp4ToAnnexB( const AbstractOnDemandDataProviderPtr& dataSource )
:
    AbstractMediaDataFilter( dataSource ),
    m_isFirstPacket( true )
{
}

QnAbstractDataPacketPtr H264Mp4ToAnnexB::processData( QnAbstractDataPacketPtr* const data )
{
    static const quint8 START_CODE[] = { 0, 0, 0, 1 };
    QnCompressedVideoData* srcVideoPacket = dynamic_cast<QnCompressedVideoData*>(data->get());
    if( !srcVideoPacket || srcVideoPacket->compressionType != AV_CODEC_ID_H264 )
        return *data;
    if(srcVideoPacket->dataSize() >= sizeof(START_CODE) && srcVideoPacket->data()[3] == 1)
        return *data;   //already in annexb format. TODO #ak: format validation is too weak

    //copying packet srcVideoPacket to videoPacket
    //TODO #ak not good, but with current implementation of buffer we can do nothing better
    QnWritableCompressedVideoDataPtr videoPacket = QnWritableCompressedVideoDataPtr(static_cast<QnWritableCompressedVideoData*>(srcVideoPacket->clone()));

    //replacing NALU size with {0 0 0 1}
    for( quint8* dataStart = (quint8*)videoPacket->m_data.data();
        dataStart < (quint8*)videoPacket->m_data.data() + videoPacket->m_data.size();
         )
    {
        size_t naluSize = ntohl( *(u_long*)dataStart );
        memcpy( dataStart, START_CODE, sizeof(START_CODE) );
        dataStart += sizeof(START_CODE) + naluSize;
    }

    if( m_isFirstPacket && isH264SeqHeaderInExtraData( videoPacket ) )
    {
        m_newContext = QnConstMediaContextPtr(!videoPacket->context? nullptr :
            videoPacket->context->cloneWithoutExtradata());

        //reading sequence header from extradata
        std::basic_string<quint8> seqHeader;
        readH264SeqHeaderFromExtraData( videoPacket, &seqHeader );
        if( seqHeader.size() > 0 )
        {
            QnByteArray mediaDataWithSeqHeader( FF_INPUT_BUFFER_PADDING_SIZE, static_cast<unsigned int>(seqHeader.size() + videoPacket->dataSize()) );
            mediaDataWithSeqHeader.resize( static_cast<unsigned int>(seqHeader.size() + videoPacket->dataSize()) );
            memcpy( mediaDataWithSeqHeader.data(), seqHeader.data(), seqHeader.size() );
            memcpy( mediaDataWithSeqHeader.data() + seqHeader.size(), videoPacket->data(), videoPacket->dataSize() );
            videoPacket->m_data = mediaDataWithSeqHeader;
            m_isFirstPacket = false;

            //TODO #ak: monitor sequence header change
        }
    }

    videoPacket->context = m_newContext;

    return videoPacket;
}

//dishonorably stolen from libavcodec source
#ifndef AV_RB16
#   define AV_RB16(x)                           \
    ((((const uint8_t*)(x))[0] << 8) |          \
      ((const uint8_t*)(x))[1])
#endif
static const quint8 H264_START_CODE[] = { 0, 0, 0, 1 };

bool H264Mp4ToAnnexB::isH264SeqHeaderInExtraData( const QnAbstractMediaDataPtr& data ) const
{
    return data->context &&
        data->context->getExtradataSize() >= 7 &&
        data->context->getExtradata()[0] == 1;
}

// TODO: Code duplication with "h264_utils.cpp".
/**
 * @param data data->context should not be null.
 */
void H264Mp4ToAnnexB::readH264SeqHeaderFromExtraData(
    const QnAbstractMediaDataPtr& data,
    std::basic_string<quint8>* const seqHeader )
{
    NX_ASSERT(data->context);
    const unsigned char* p = data->context->getExtradata();

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

    for (int i = 0; i < cnt; i++)
    {
        const int nalsize = AV_RB16(p);
        p += 2; //skipping nalusize
        if (nalsize > data->context->getExtradataSize() - (p - data->context->getExtradata()))
            break;
        seqHeader->append( H264_START_CODE, sizeof(H264_START_CODE) );
        seqHeader->append( p, nalsize );
        p += nalsize;
    }

    // Decode pps from avcC
    cnt = *(p++); // Number of pps
    for (int i = 0; i < cnt; ++i)
    {
        const int nalsize = AV_RB16(p);
        p += 2;
        if (nalsize > data->context->getExtradataSize() - (p - data->context->getExtradata()))
            break;
        seqHeader->append( H264_START_CODE, sizeof(H264_START_CODE) );
        seqHeader->append( p, nalsize );
        p += nalsize;
    }
}

#endif // ENABLE_DATA_PROVIDERS
