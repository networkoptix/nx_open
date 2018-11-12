#include "frame_type_extractor.h"

#include "vc1Parser.h"
#include <utils/media/nalUnits.h>

void FrameTypeExtractor::decodeWMVSequence(const quint8* data, int size)
{
    m_vcSequence = new VC1SequenceHeader();
    try {
        m_vcSequence->vc1_unescape_buffer(data, size);
        if (m_vcSequence->decode_sequence_header() != 0) {
            delete m_vcSequence;
            m_vcSequence = 0;
        }
    } catch (...) {
        delete m_vcSequence;
        m_vcSequence = 0;
    }
}

FrameTypeExtractor::FrameTypeExtractor(const QnConstMediaContextPtr& context):
    m_context(context),
    m_codecId(context->getCodecId()),
    m_vcSequence(0),
    m_dataWithNalPrefixes(false)
{
    if (context && context->getExtradataSize() > 0)
    {
        const quint8* data = context->getExtradata();
        int size = context->getExtradataSize();
        if (m_codecId == AV_CODEC_ID_VC1)
        {
            for (int i = 0; i < size - 4; ++i)
            {
                if (data[i + 0] == 0 &&
                    data[i + 1] == 0 &&
                    data[i + 2] == 1 &&
                    data[i + 3] == VC1_CODE_SEQHDR)
                {
                    decodeWMVSequence(data + i + 4, size - i - 4);
                    break;
                }
            }
        }
        else if (m_codecId == AV_CODEC_ID_WMV1 || m_codecId == AV_CODEC_ID_WMV2 || m_codecId == AV_CODEC_ID_WMV3)
        {
            decodeWMVSequence(data, size);
        }
        else if (m_codecId == AV_CODEC_ID_H264)
        {
            m_dataWithNalPrefixes = data[0] == 0;
        }
    }
}

FrameTypeExtractor::FrameTypeExtractor(AVCodecID codecId, bool nalPrefixes):
    m_context(nullptr),
    m_codecId(codecId),
    m_vcSequence(nullptr),
    m_dataWithNalPrefixes(nalPrefixes)
{
}

FrameTypeExtractor::~FrameTypeExtractor()
{
    delete m_vcSequence;
}

FrameTypeExtractor::FrameType FrameTypeExtractor::getH264FrameType(
    const quint8* data, int size)
{
    if (size < 4)
        return UnknownFrameType;
    const quint8* end = data + size;
    while (data < end)
    {
        if (m_dataWithNalPrefixes)
            data = NALUnit::findNextNAL(data, end);
        else
            data += 4;
        if (data >= end)
            break;

        quint8 nalType = *data & 0x1f;
        if (nalType >= nuSliceNonIDR && nalType <= nuSliceIDR)
        {
            if (nalType == nuSliceIDR)
                return I_Frame;
            quint8 nal_ref_idc = (*data >> 5) & 3;
            if (nal_ref_idc)
                return P_Frame;

            BitStreamReader bitReader;
            bitReader.setBuffer(data + 1, end);
            try {
                /*int first_mb_in_slice =*/ NALUnit::extractUEGolombCode(bitReader);

                int slice_type = NALUnit::extractUEGolombCode(bitReader);
                if (slice_type >= 5)
                    slice_type -= 5; //< +5 flag is: all other slices at this picture must be of the same type.

                if (slice_type == SliceUnit::I_TYPE || slice_type == SliceUnit::SI_TYPE)
                    return P_Frame; //< Fake. It is i-frame, but not IDR, so we can't seek to this time and etc.  // I_Frame;
                else if (slice_type == SliceUnit::P_TYPE || slice_type == SliceUnit::SP_TYPE)
                    return P_Frame;
                else if (slice_type == SliceUnit::B_TYPE)
                    return B_Frame;
                else
                    return UnknownFrameType;
            } catch(...) {
                return UnknownFrameType;
            }
        }
        if (!m_dataWithNalPrefixes)
            break;
    }
    return UnknownFrameType;
}

FrameTypeExtractor::FrameType FrameTypeExtractor::getMpegVideoFrameType(
    const quint8* data, int size)
{
    enum PictureCodingType { PCT_FORBIDDEN, PCT_I_FRAME, PCT_P_FRAME, PCT_B_FRAME, PCT_D_FRAME };
    const quint8* end = data + size;
    while (data <= end-4 && data[3] > 0x80)
    {
        data = NALUnit::findNextNAL(data + 4, end);
        if (data == end)
            return UnknownFrameType;
        data -= 3;
        size = end - data;
    }
    if (size > 5)
    {
        int frameType = (data[5] >> 3) & 7;
        if (frameType == PCT_I_FRAME)
            return I_Frame;
        else if (frameType == PCT_P_FRAME)
            return P_Frame;
        else if (frameType == PCT_B_FRAME)
            return B_Frame;
    }
    return UnknownFrameType;
}

FrameTypeExtractor::FrameType FrameTypeExtractor::getWMVFrameType(
    const quint8* data, int size)
{
    if (m_vcSequence) {
        VC1Frame frame;
        if (frame.decode_frame_direct(*m_vcSequence, data, data + size) == 0)
        {
            if (frame.pict_type == VC_I_TYPE)
                return I_Frame;
            else if (frame.pict_type == VC_P_TYPE)
                return P_Frame;
            else
                return B_Frame;
        }
    }
    return UnknownFrameType;
}

FrameTypeExtractor::FrameType FrameTypeExtractor::getVCFrameType(
    const quint8* data, int size)
{
    if (data[3] == VC1_CODE_FRAME ||
        (data[3] == VC1_USER_CODE_FRAME && m_vcSequence))
    {
        return getWMVFrameType(data + 4, size - 4);
    }
    else if (data[3] == VC1_CODE_SEQHDR)
    {
        return I_Frame;
#if 0
        const quint8* next = NALUnit::findNextNAL(data + 3, data + size);
        if (next < data+size)
        {
            //m_vcSequence->vc1_unescape_buffer(data + 4, next - (data + 4));
            //m_vcSequence->decode_sequence_header();
            FrameTypeExtractor::FrameType rez =
                getWMVFrameType(next, size - (next - data));
            return rez;
        }
#endif // 0
    }
    return UnknownFrameType;
}

FrameTypeExtractor::FrameType FrameTypeExtractor::getMpeg4FrameType(
    const quint8* data, int size)
{
    enum PictureCodingType { PCT_I_FRAME, PCT_P_FRAME, PCT_B_FRAME, PCT_S_FRAME };

    const quint8* end = data + size;
    while (data <= end - 4 && data[3] != 0xb6)
    {
        data = NALUnit::findNextNAL(data + 4, end);
        if (data == end)
            return UnknownFrameType;
        data -= 3;
        size = end - data;
    }
    if (size >= 5)
    {
        int frameType = data[4] >> 6;
        if (frameType == PCT_I_FRAME)
            return I_Frame;
        else if (frameType == PCT_P_FRAME)
            return P_Frame;
        else if (frameType == PCT_B_FRAME)
            return B_Frame;
    }
    return UnknownFrameType;

#if 0
    if (size >= 1)
    {
        int frameType = data[0] >> 6;
        if (frameType == PCT_I_FRAME)
            return I_Frame;
        else if (frameType == PCT_P_FRAME)
            return P_Frame;
        else if (frameType == PCT_B_FRAME)
            return B_Frame;
    }
    return UnknownFrameType;
#endif // 0
}

FrameTypeExtractor::FrameType FrameTypeExtractor::getFrameType(
    const quint8* data, int dataLen)
{
    if (!data)
        return UnknownFrameType;
    switch (m_codecId)
    {
        case AV_CODEC_ID_H264:
            return getH264FrameType(data, dataLen);
        case AV_CODEC_ID_MPEG1VIDEO:
        case AV_CODEC_ID_MPEG2VIDEO:
            return getMpegVideoFrameType(data, dataLen);
        case AV_CODEC_ID_WMV1:
        case AV_CODEC_ID_WMV2:
        case AV_CODEC_ID_WMV3:
            return getWMVFrameType(data, dataLen);
        case AV_CODEC_ID_VC1:
            return getVCFrameType(data, dataLen);
        case AV_CODEC_ID_MPEG4:
            return getMpeg4FrameType(data, dataLen);
        default:
            return UnknownFrameType;
    }
}
