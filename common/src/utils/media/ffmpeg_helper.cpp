#include "ffmpeg_helper.h"
#include "nalUnits.h"
#include "bitStream.h"
#include "vc1Parser.h"

extern QMutex global_ffmpeg_mutex;

static inline QByteArray codecIDToByteArray(CodecID codecID)
{
    switch(codecID)
    {
        /* video codecs */
    case CODEC_ID_MPEG1VIDEO: return "MPEG1VIDEO";
    case CODEC_ID_MPEG2VIDEO: return "MPEG2VIDEO"; ///< preferred ID for MPEG-1/2 video decoding
    case CODEC_ID_MPEG2VIDEO_XVMC: return "MPEG2VIDEO-XVMC";
    case CODEC_ID_H261: return "H261";
    case CODEC_ID_H263: return "H263";
    case CODEC_ID_RV10: return "RV10";
    case CODEC_ID_RV20: return "RV20";
    case CODEC_ID_MJPEG: return "MJPEG";
    case CODEC_ID_MJPEGB: return "MJPEGB";
    case CODEC_ID_LJPEG: return "LJPEG";
    case CODEC_ID_SP5X: return "SP5X";
    case CODEC_ID_JPEGLS: return "JPEGLS";
    case CODEC_ID_MPEG4: return "MPEG4";
    case CODEC_ID_RAWVIDEO: return "RAWVIDEO";
    case CODEC_ID_MSMPEG4V1: return "MSMPEG4V1";
    case CODEC_ID_MSMPEG4V2: return "MSMPEG4V2";
    case CODEC_ID_MSMPEG4V3: return "MSMPEG4V3";
    case CODEC_ID_WMV1: return "WMV1";
    case CODEC_ID_WMV2: return "WMV2";
    case CODEC_ID_H263P: return "H263P";
    case CODEC_ID_H263I: return "H263I";
    case CODEC_ID_FLV1: return "FLV1";
    case CODEC_ID_SVQ1: return "SVQ1";
    case CODEC_ID_SVQ3: return "SVQ3";
    case CODEC_ID_DVVIDEO: return "DVVIDEO";
    case CODEC_ID_HUFFYUV: return "HUFFYUV";
    case CODEC_ID_CYUV: return "CYUV";
    case CODEC_ID_H264: return "H264";
    case CODEC_ID_INDEO3: return "INDEO3";
    case CODEC_ID_VP3: return "VP3";
    case CODEC_ID_THEORA: return "THEORA";
    case CODEC_ID_ASV1: return "ASV1";
    case CODEC_ID_ASV2: return "ASV2";
    case CODEC_ID_FFV1: return "FFV1";
    case CODEC_ID_4XM: return "4XM";
    case CODEC_ID_VCR1: return "VCR1";
    case CODEC_ID_CLJR: return "CLJR";
    case CODEC_ID_MDEC: return "MDEC";
    case CODEC_ID_ROQ: return "ROQ";
    case CODEC_ID_INTERPLAY_VIDEO: return "INTERPLAY_VIDEO";
    case CODEC_ID_XAN_WC3: return "XAN_WC3";
    case CODEC_ID_XAN_WC4: return "XAN_WC4";
    case CODEC_ID_RPZA: return "RPZA";
    case CODEC_ID_CINEPAK: return "CINEPAK";
    case CODEC_ID_WS_VQA: return "WS_VQA";
    case CODEC_ID_MSRLE: return "MSRLE";
    case CODEC_ID_MSVIDEO1: return "MSVIDEO1";
    case CODEC_ID_IDCIN: return "IDCIN";
    case CODEC_ID_8BPS: return "8BPS";
    case CODEC_ID_SMC: return "SMC";
    case CODEC_ID_FLIC: return "FLIC";
    case CODEC_ID_TRUEMOTION1: return "TRUEMOTION1";
    case CODEC_ID_VMDVIDEO: return "VMDVIDEO";
    case CODEC_ID_MSZH: return "MSZH";
    case CODEC_ID_ZLIB: return "ZLIB";
    case CODEC_ID_QTRLE: return "QTRLE";
    case CODEC_ID_SNOW: return "SNOW";
    case CODEC_ID_TSCC: return "TSCC";
    case CODEC_ID_ULTI: return "ULTI";
    case CODEC_ID_QDRAW: return "QDRAW";
    case CODEC_ID_VIXL: return "VIXL";
    case CODEC_ID_QPEG: return "QPEG";
    case CODEC_ID_PNG: return "PNG";
    case CODEC_ID_PPM: return "PPM";
    case CODEC_ID_PBM: return "PBM";
    case CODEC_ID_PGM: return "PGM";
    case CODEC_ID_PGMYUV: return "PGMYUV";
    case CODEC_ID_PAM: return "PAM";
    case CODEC_ID_FFVHUFF: return "FFVHUFF";
    case CODEC_ID_RV30: return "RV30";
    case CODEC_ID_RV40: return "RV40";
    case CODEC_ID_VC1: return "VC1";
    case CODEC_ID_WMV3: return "WMV3";
    case CODEC_ID_LOCO: return "LOCO";
    case CODEC_ID_WNV1: return "WNV1";
    case CODEC_ID_AASC: return "AASC";
    case CODEC_ID_INDEO2: return "INDEO2";
    case CODEC_ID_FRAPS: return "FRAPS";
    case CODEC_ID_TRUEMOTION2: return "TRUEMOTION2";
    case CODEC_ID_BMP: return "BMP";
    case CODEC_ID_CSCD: return "CSCD";
    case CODEC_ID_MMVIDEO: return "MMVIDEO";
    case CODEC_ID_ZMBV: return "ZMBV";
    case CODEC_ID_AVS: return "AVS";
    case CODEC_ID_SMACKVIDEO: return "SMACKVIDEO";
    case CODEC_ID_NUV: return "NUV";
    case CODEC_ID_KMVC: return "KMVC";
    case CODEC_ID_FLASHSV: return "FLASHSV";
    case CODEC_ID_CAVS: return "CAVS";
    case CODEC_ID_JPEG2000: return "JPEG2000";
    case CODEC_ID_VMNC: return "VMNC";
    case CODEC_ID_VP5: return "VP5";
    case CODEC_ID_VP6: return "VP6";
    case CODEC_ID_VP6F: return "VP6F";
    case CODEC_ID_TARGA: return "TARGA";
    case CODEC_ID_DSICINVIDEO: return "DSICINVIDEO";
    case CODEC_ID_TIERTEXSEQVIDEO: return "TIERTEXSEQVIDEO";
    case CODEC_ID_TIFF: return "TIFF";
    case CODEC_ID_GIF: return "GIF";
    case CODEC_ID_FFH264: return "FFH264";
    case CODEC_ID_DXA: return "DXA";
    case CODEC_ID_DNXHD: return "DNXHD";
    case CODEC_ID_THP: return "THP";
    case CODEC_ID_SGI: return "SGI";
    case CODEC_ID_C93: return "C93";
    case CODEC_ID_BETHSOFTVID: return "BETHSOFTVID";
    case CODEC_ID_PTX: return "PTX";
    case CODEC_ID_TXD: return "TXD";
    case CODEC_ID_VP6A: return "VP6A";
    case CODEC_ID_AMV: return "AMV";
    case CODEC_ID_VB: return "VB";
    case CODEC_ID_PCX: return "PCX";
    case CODEC_ID_SUNRAST: return "SUNRAST";
    case CODEC_ID_INDEO4: return "INDEO4";
    case CODEC_ID_INDEO5: return "INDEO5";
    case CODEC_ID_MIMIC: return "MIMIC";
    case CODEC_ID_RL2: return "RL2";
    case CODEC_ID_8SVX_EXP: return "8SVX_EXP";
    case CODEC_ID_8SVX_FIB: return "8SVX_FIB";
    case CODEC_ID_ESCAPE124: return "ESCAPE124";
    case CODEC_ID_DIRAC: return "DIRAC";
    case CODEC_ID_BFI: return "BFI";
    case CODEC_ID_CMV: return "CMV";
    case CODEC_ID_MOTIONPIXELS: return "MOTIONPIXELS";
    case CODEC_ID_TGV: return "TGV";
    case CODEC_ID_TGQ: return "TGQ";
    case CODEC_ID_TQI: return "TQI";
    case CODEC_ID_AURA: return "AURA";
    case CODEC_ID_AURA2: return "AURA2";
    case CODEC_ID_V210X: return "V210X";
    case CODEC_ID_TMV: return "TMV";
    case CODEC_ID_V210: return "V210";
    case CODEC_ID_DPX: return "DPX";
    case CODEC_ID_MAD: return "MAD";
    case CODEC_ID_FRWU: return "FRWU";
    case CODEC_ID_FLASHSV2: return "FLASHSV2";
    case CODEC_ID_CDGRAPHICS: return "CDGRAPHICS";
    case CODEC_ID_R210: return "R210";
    case CODEC_ID_ANM: return "ANM";
    case CODEC_ID_BINKVIDEO: return "BINKVIDEO";
    case CODEC_ID_IFF_ILBM: return "IFF_ILBM";
    case CODEC_ID_IFF_BYTERUN1: return "BYTERUN1";
    case CODEC_ID_KGV1: return "KGV1";
    case CODEC_ID_YOP: return "YOP";
    case CODEC_ID_VP8: return "VP8";
    case CODEC_ID_PICTOR: return "PICTOR";
    case CODEC_ID_ANSI: return "ANSI";
    case CODEC_ID_A64_MULTI: return "A64_MULTI";
    case CODEC_ID_A64_MULTI5: return "A64_MULTI5";
    case CODEC_ID_R10K: return "R10K";
    case CODEC_ID_MXPEG: return "MXPEG";
    case CODEC_ID_LAGARITH: return "LAGARITH";
    case CODEC_ID_PRORES: return "PRORES";
    case CODEC_ID_JV: return "JV";
    case CODEC_ID_DFA: return "DFA";
    case CODEC_ID_8SVX_RAW: return "8SVX_RAW";

        /* various PCM "codecs" */
    case CODEC_ID_PCM_S16LE:
    case CODEC_ID_PCM_S16BE:
    case CODEC_ID_PCM_U16LE:
    case CODEC_ID_PCM_U16BE:
    case CODEC_ID_PCM_S8:
    case CODEC_ID_PCM_U8:
    case CODEC_ID_PCM_MULAW:
    case CODEC_ID_PCM_ALAW:
    case CODEC_ID_PCM_S32LE:
    case CODEC_ID_PCM_S32BE:
    case CODEC_ID_PCM_U32LE:
    case CODEC_ID_PCM_U32BE:
    case CODEC_ID_PCM_S24LE:
    case CODEC_ID_PCM_S24BE:
    case CODEC_ID_PCM_U24LE:
    case CODEC_ID_PCM_U24BE:
    case CODEC_ID_PCM_S24DAUD:
    case CODEC_ID_PCM_ZORK:
    case CODEC_ID_PCM_S16LE_PLANAR:
    case CODEC_ID_PCM_DVD:
    case CODEC_ID_PCM_F32BE:
    case CODEC_ID_PCM_F32LE:
    case CODEC_ID_PCM_F64BE:
    case CODEC_ID_PCM_F64LE:
    case CODEC_ID_PCM_BLURAY:
    case CODEC_ID_PCM_LXF:
    case CODEC_ID_S302M:

        /* various ADPCM codecs */
    case CODEC_ID_ADPCM_IMA_QT:
    case CODEC_ID_ADPCM_IMA_WAV:
    case CODEC_ID_ADPCM_IMA_DK3:
    case CODEC_ID_ADPCM_IMA_DK4:
    case CODEC_ID_ADPCM_IMA_WS:
    case CODEC_ID_ADPCM_IMA_SMJPEG:
    case CODEC_ID_ADPCM_MS:
    case CODEC_ID_ADPCM_4XM:
    case CODEC_ID_ADPCM_XA:
    case CODEC_ID_ADPCM_ADX:
    case CODEC_ID_ADPCM_EA:
    case CODEC_ID_ADPCM_G726:
    case CODEC_ID_ADPCM_CT:
    case CODEC_ID_ADPCM_SWF:
    case CODEC_ID_ADPCM_YAMAHA:
    case CODEC_ID_ADPCM_SBPRO_4:
    case CODEC_ID_ADPCM_SBPRO_3:
    case CODEC_ID_ADPCM_SBPRO_2:
    case CODEC_ID_ADPCM_THP:
    case CODEC_ID_ADPCM_IMA_AMV:
    case CODEC_ID_ADPCM_EA_R1:
    case CODEC_ID_ADPCM_EA_R3:
    case CODEC_ID_ADPCM_EA_R2:
    case CODEC_ID_ADPCM_IMA_EA_SEAD:
    case CODEC_ID_ADPCM_IMA_EA_EACS:
    case CODEC_ID_ADPCM_EA_XAS:
    case CODEC_ID_ADPCM_EA_MAXIS_XA:
    case CODEC_ID_ADPCM_IMA_ISS:
    case CODEC_ID_ADPCM_G722:
        return "PCM";

        /* AMR */
    case CODEC_ID_AMR_NB: return "AMR-NB";
    case CODEC_ID_AMR_WB: return "AMR-WB";

        /* RealAudio codecs*/
    case CODEC_ID_RA_144: return "RA-144";
    case CODEC_ID_RA_288: return "RA-288";

        /* various DPCM codecs */
    case CODEC_ID_ROQ_DPCM:
    case CODEC_ID_INTERPLAY_DPCM:
    case CODEC_ID_XAN_DPCM:
    case CODEC_ID_SOL_DPCM:
        return "PCM";

        /* audio codecs */
    case CODEC_ID_MP2: return "MP2";
    case CODEC_ID_MP3: return "MP3"; ///< preferred ID for decoding MPEG audio layer 1: return ""; 2 or 3
    case CODEC_ID_AAC: return "AAC";
    case CODEC_ID_AC3: return "Dolby";
    case CODEC_ID_DTS: return "DTS";
    case CODEC_ID_VORBIS: return "VORBIS";
    case CODEC_ID_DVAUDIO: return "DVAUDIO";
    case CODEC_ID_WMAV1: return "WMAV1";
    case CODEC_ID_WMAV2: return "WMAV2";
    case CODEC_ID_MACE3: return "MACE3";
    case CODEC_ID_MACE6: return "MACE6";
    case CODEC_ID_VMDAUDIO: return "VMDAUDIO";
    case CODEC_ID_SONIC: return "SONIC";
    case CODEC_ID_SONIC_LS: return "SONIC_LC";
    case CODEC_ID_FLAC: return "FLAC";
    case CODEC_ID_MP3ADU: return "MP3";
    case CODEC_ID_MP3ON4: return "MP3";
    case CODEC_ID_SHORTEN: return "SHORTEN";
    case CODEC_ID_ALAC: return "ALAC";
    case CODEC_ID_WESTWOOD_SND1: return "WESTWOOD";
    case CODEC_ID_GSM: return "GSM"; ///< as in Berlin toast format
    case CODEC_ID_QDM2: return "QDM2";
    case CODEC_ID_COOK: return "COOK";
    case CODEC_ID_TRUESPEECH: return "TRUESPEECH";
    case CODEC_ID_TTA: return "TTA";
    case CODEC_ID_SMACKAUDIO: return "SMACKAUDIO";
    case CODEC_ID_QCELP: return "QCELP";
    case CODEC_ID_WAVPACK: return "WAVPACK";
    case CODEC_ID_DSICINAUDIO: return "DSICINAUDIO";
    case CODEC_ID_IMC: return "IMC";
    case CODEC_ID_MUSEPACK7: return "MUSEPACK7";
    case CODEC_ID_MLP: return "MLP";
    case CODEC_ID_GSM_MS: return "GSM_MS"; /* as found in WAV */
    case CODEC_ID_ATRAC3: return "ATRAC3";
    case CODEC_ID_VOXWARE: return "VOXWARE";
    case CODEC_ID_APE: return "APE";
    case CODEC_ID_NELLYMOSER: return "NELLYMOSER";
    case CODEC_ID_MUSEPACK8: return "MUSEPACK8";
    case CODEC_ID_SPEEX: return "SPEEX";
    case CODEC_ID_WMAVOICE: return "WMA VOICE";
    case CODEC_ID_WMAPRO: return "WMA PRO";
    case CODEC_ID_WMALOSSLESS: return "WMA LOSSLESS";
    case CODEC_ID_ATRAC3P: return "ATRAC3P";
    case CODEC_ID_EAC3: return "EAC3";
    case CODEC_ID_SIPR: return "SIPR";
    case CODEC_ID_MP1: return "MP1";
    case CODEC_ID_TWINVQ: return "TWINVQ";
    case CODEC_ID_TRUEHD: return "TRUE-HD";
    case CODEC_ID_MP4ALS: return "MP4ALS";
    case CODEC_ID_ATRAC1: return "ATRAC1";
    case CODEC_ID_BINKAUDIO_RDFT: return "BINKAUDIO RDFT";
    case CODEC_ID_BINKAUDIO_DCT: return "BINKAUDIO DCT";
    case CODEC_ID_AAC_LATM: return "AAC LATM";
    case CODEC_ID_QDMC: return "QDMC";
    case CODEC_ID_CELT: return "CELT";

        /* subtitle codecs */
    case CODEC_ID_DVD_SUBTITLE: return "DVD_SUBTITLE";
    case CODEC_ID_DVB_SUBTITLE: return "DVB_SUBTITLE";
    case CODEC_ID_TEXT: return "TEXT";  ///< raw UTF-8 text
    case CODEC_ID_XSUB: return "XSUB";
    case CODEC_ID_SSA: return "SSA";
    case CODEC_ID_MOV_TEXT: return "MOV_TEXT";
    case CODEC_ID_HDMV_PGS_SUBTITLE: return "HDMV_PGS_SUBTITLE";
    case CODEC_ID_DVB_TELETEXT: return "DVB_TELETEXT";
    case CODEC_ID_SRT: return "SRT";
    case CODEC_ID_MICRODVD: return "MICRODVD";

        /* other specific kind of codecs (generally used for attachments) */
    case CODEC_ID_TTF: return "TTF";

    case CODEC_ID_PROBE: return "PROBE"; ///< codec_id is not known (like CODEC_ID_NONE) but lavf should attempt to identify it

    case CODEC_ID_MPEG2TS: return "MPEG2TS"; /**< _FAKE_ codec to indicate a raw MPEG-2 TS
                                      * stream (only used by libavformat) */
    case CODEC_ID_FFMETADATA: return "FFMETADATA";   ///< Dummy codec for streams containing only metadata information.

    case CODEC_ID_NONE:
    default:
        break;
    };

    return 0;
}

QString codecIDToString(CodecID codecID)
{
    return QString::fromLatin1(codecIDToByteArray(codecID));
}

// ------------------------- FrameTypeExtractor -------------------------

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

FrameTypeExtractor::FrameTypeExtractor(AVCodecContext* context): 
    m_context(context), 
    m_codecId(context->codec_id),
    m_vcSequence(0),
    m_dataWithNalPrefixes(false)
{
    if (m_context && context->extradata_size > 0)
    {
        quint8* data = context->extradata;
        if (m_codecId == CODEC_ID_VC1) 
        {
            for (int i = 0; i < context->extradata_size-4; ++i)
            {
                if (data[i] == 0 && data[i+1] == 0 && data[i+2] == 1 && data[i+3] == VC1_CODE_SEQHDR) 
                {
                    decodeWMVSequence(data + i+ 4, context->extradata_size - i - 4);
                    break;
                }
            }
        }
        else if (m_codecId == CODEC_ID_WMV1 || m_codecId == CODEC_ID_WMV2 || m_codecId == CODEC_ID_WMV3) 
        {
            decodeWMVSequence(data, context->extradata_size);
        }
        else if (m_codecId == CODEC_ID_H264) {
            m_dataWithNalPrefixes = context->extradata[0] == 0;
        }

    }
}

FrameTypeExtractor::~FrameTypeExtractor()
{
    delete m_vcSequence;
};

FrameTypeExtractor::FrameType FrameTypeExtractor::getH264FrameType(const quint8* data, int size)
{
    if (size < 4)
        return UnknownFrameType;
    const quint8* end = data + size;
    while (data < end)
    {
        if (m_dataWithNalPrefixes) {
            data = NALUnit::findNextNAL(data, end);
            if (data >= end)
                break;
        }
        else {
            data += 4;
        }
        quint8 nalType = *data & 0x1f;
        if (nalType >= nuSliceNonIDR && nalType <= nuSliceIDR)
        {
            if (nalType == nuSliceIDR)
                return I_Frame;
            quint8 nal_ref_idc = (*data >> 5) & 3;
            if (nal_ref_idc)
                return P_Frame;

            BitStreamReader bitReader;
            bitReader.setBuffer(data+1, end);
            try {
                int first_mb_in_slice = NALUnit::extractUEGolombCode(bitReader);
                int slice_type = NALUnit::extractUEGolombCode(bitReader);
                if (slice_type >= 5)
                    slice_type -= 5; // +5 flag is: all other slice at this picture must be same type

                if (slice_type == SliceUnit::I_TYPE || slice_type == SliceUnit::SI_TYPE) 
                    return P_Frame; // fake. It is i-frame, but not IDR, so we can't seek to this time and e.t.c.  // I_Frame;
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

FrameTypeExtractor::FrameType FrameTypeExtractor::getMpegVideoFrameType(const quint8* data, int size)
{
    enum PictureCodingType {PCT_FORBIDDEN, PCT_I_FRAME, PCT_P_FRAME, PCT_B_FRAME, PCT_D_FRAME};
    const quint8* end = data + size;
    while (data <= end-4 && data[3] > 0x80) 
    {
        data = NALUnit::findNextNAL(data+4, end);
        if (data == end)
            return UnknownFrameType;
        data -=3;
        size = end - data;
    }
    if (size >= 5) 
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

FrameTypeExtractor::FrameType FrameTypeExtractor::getWMVFrameType(const quint8* data, int size)
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

FrameTypeExtractor::FrameType FrameTypeExtractor::getVCFrameType(const quint8* data, int size)
{
    if (data[3] == VC1_CODE_FRAME || data[3] == VC1_USER_CODE_FRAME && m_vcSequence)
        return getWMVFrameType(data+4, size-4);
    else if (data[3] == VC1_CODE_SEQHDR) 
    {
        return I_Frame;
        /*
        const quint8* next = NALUnit::findNextNAL(data+3, data+size);
        if (next < data+size) 
        {
            //m_vcSequence->vc1_unescape_buffer(data+4, next - (data+4));
            //m_vcSequence->decode_sequence_header();
            FrameTypeExtractor::FrameType rez = getWMVFrameType(next, size - (next - data));
            return rez;
        }
        */
    }
    return UnknownFrameType;
}

FrameTypeExtractor::FrameType FrameTypeExtractor::getMpeg4FrameType(const quint8* data, int size)
{
    enum PictureCodingType {PCT_I_FRAME, PCT_P_FRAME, PCT_B_FRAME, PCT_S_FRAME};

    const quint8* end = data + size;
    while (data <= end-4 && data[3] != 0xb6)
    {
        data = NALUnit::findNextNAL(data+4, end);
        if (data == end)
            return UnknownFrameType;
        data -=3;
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

    /*
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
    */
}

FrameTypeExtractor::FrameType FrameTypeExtractor::getFrameType(const quint8* data, int dataLen)
{
    //quint32 size = ntohl( *((quint32*) data));
    switch (m_codecId)
    {
        case CODEC_ID_H264:
            return getH264FrameType(data, dataLen);
        case CODEC_ID_MPEG1VIDEO:
        case CODEC_ID_MPEG2VIDEO:
            return getMpegVideoFrameType(data, dataLen);
        case CODEC_ID_WMV1:
        case CODEC_ID_WMV2:
        case CODEC_ID_WMV3:
            return getWMVFrameType(data, dataLen);
        case CODEC_ID_VC1:
            return getVCFrameType(data, dataLen);
        case CODEC_ID_MPEG4:
            return getMpeg4FrameType(data, dataLen);
    }
    return UnknownFrameType;
}

void QnFfmpegHelper::serializeCodecContext(const AVCodecContext *ctx, QByteArray *data)
{
    data->clear();

    // serialize context in way as avcodec_copy_context is works
    AVCodecContext dest;
    memcpy(&dest, ctx, sizeof(AVCodecContext));

    dest.av_class = NULL;
    dest.get_buffer = NULL;
    dest.reget_buffer = NULL;
    dest.get_format = NULL;
    dest.release_buffer = NULL;
    dest.execute = NULL;
    dest.execute2 = NULL;


    dest.priv_data       = NULL;
    dest.codec           = NULL;
    dest.palctrl         = NULL;
    dest.slice_offset    = NULL;
    dest.internal_buffer = NULL;
    dest.hwaccel         = NULL;
    dest.thread_opaque   = NULL;

    data->append((const char*) &dest, sizeof(AVCodecContext));
    if (ctx->rc_eq)
        data->append(ctx->rc_eq);
    if (ctx->extradata)
        data->append((const char*) ctx->extradata, ctx->extradata_size);
    if (ctx->intra_matrix)
        data->append((const char*) ctx->intra_matrix, 64 * sizeof(int16_t));
    if (ctx->inter_matrix)
        data->append((const char*) ctx->inter_matrix, 64 * sizeof(int16_t));
    if (ctx->rc_override)
        data->append((const char*) ctx->rc_override, ctx->rc_override_count * sizeof(*ctx->rc_override));
}

AVCodecContext *QnFfmpegHelper::deserializeCodecContext(const char *data, int dataLen)
{
    if (dataLen < sizeof(AVCodecContext))
    {
        qWarning() << Q_FUNC_INFO << __LINE__ << "Too few data for deserialize CodecContext";
        return 0;
    }

    QMutexLocker lock(&global_ffmpeg_mutex);
    
    AVCodecContext* ctx = (AVCodecContext*) avcodec_alloc_context();
    AVCodecContext tmp;
    memcpy(&tmp, ctx, sizeof(AVCodecContext));
    memcpy(ctx, data, sizeof(AVCodecContext));

    ctx->av_class = tmp.av_class;
    ctx->get_buffer = tmp.get_buffer;
    ctx->reget_buffer = tmp.reget_buffer;
    ctx->get_format = tmp.get_format;
    ctx->release_buffer = tmp.release_buffer;
    ctx->execute = tmp.execute;
    ctx->execute2 = tmp.execute2;


    dataLen -= sizeof(AVCodecContext);
    data += sizeof(AVCodecContext);

    if (ctx->rc_eq && dataLen > 0)
    {
        int len = qstrnlen(data, dataLen);
        if (data[len] != 0)
        {
            qWarning() << Q_FUNC_INFO << __LINE__ << "Too few data for deserialize CodecContext";
            av_free(ctx);
            return 0;
        }
        ctx->rc_eq = av_strdup(data);
        data += len;
        dataLen -= len;
    }

    if (ctx->extradata)
    {
        if (dataLen >= ctx->extradata_size) {
            ctx->extradata = (quint8*) av_malloc(ctx->extradata_size);
            memcpy(ctx->extradata, data, ctx->extradata_size);
            data += ctx->extradata_size;
            dataLen -=  ctx->extradata_size;;
        }
        else {
            av_free((void*) ctx->rc_eq);
            av_free(ctx);
            return 0;
        }
    }

    if (ctx->intra_matrix)
    {
        if (dataLen >= 64 * sizeof(int16_t)) {
            ctx->intra_matrix = (quint16*) av_malloc(64 * sizeof(int16_t));
            memcpy(ctx->intra_matrix, data, 64 * sizeof(int16_t));
            data += 64 * sizeof(int16_t);
            dataLen -= 64 * sizeof(int16_t);
        }
        else {
            av_free(ctx->extradata);
            av_free((void*) ctx->rc_eq);
            av_free(ctx);
            return 0;
        }
    }

    if (ctx->inter_matrix)
    {
        if (dataLen >= 64 * sizeof(int16_t)) {
            ctx->inter_matrix = (quint16*) av_malloc(64 * sizeof(int16_t));
            memcpy(ctx->inter_matrix, data, 64 * sizeof(int16_t));
            data += 64 * sizeof(int16_t);
            dataLen -= 64 * sizeof(int16_t);
        }
        else {
            av_free(ctx->intra_matrix);
            av_free(ctx->extradata);
            av_free((void*) ctx->rc_eq);
            av_free(ctx);
            return 0;
        }
    }
    int ovSize = ctx->rc_override_count * sizeof(*ctx->rc_override);
    if (ctx->rc_override)
    {
        if (dataLen >= ovSize) {
            ctx->rc_override = (RcOverride*) av_malloc(ovSize);
            memcpy(ctx->rc_override, data, ovSize);
        }
        else {
            av_free(ctx->inter_matrix);
            av_free(ctx->intra_matrix);
            av_free(ctx->extradata);
            av_free((void*) ctx->rc_eq);
            av_free(ctx);
            return 0;
        }
    }
    return ctx;
}
