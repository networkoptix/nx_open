#include "ffmpeg_helper.h"

#ifdef ENABLE_DATA_PROVIDERS

#include <QtCore/QBuffer>
#include <QtCore/QDebug>

#include "nalUnits.h"
#include "bitStream.h"
#include "vc1Parser.h"
#include "core/resource/storage_resource.h"


static inline QByteArray codecIDToByteArray(AVCodecID codecID)
{
    switch(codecID)
    {
        /* video codecs */
    case AV_CODEC_ID_MPEG1VIDEO: return "MPEG1VIDEO";
    case AV_CODEC_ID_MPEG2VIDEO: return "MPEG2VIDEO"; ///< preferred ID for MPEG-1/2 video decoding
    case AV_CODEC_ID_MPEG2VIDEO_XVMC: return "MPEG2VIDEO-XVMC";
    case AV_CODEC_ID_H261: return "H261";
    case AV_CODEC_ID_H263: return "H263";
    case AV_CODEC_ID_RV10: return "RV10";
    case AV_CODEC_ID_RV20: return "RV20";
    case AV_CODEC_ID_MJPEG: return "MJPEG";
    case AV_CODEC_ID_MJPEGB: return "MJPEGB";
    case AV_CODEC_ID_LJPEG: return "LJPEG";
    case AV_CODEC_ID_SP5X: return "SP5X";
    case AV_CODEC_ID_JPEGLS: return "JPEGLS";
    case AV_CODEC_ID_MPEG4: return "MPEG4";
    case AV_CODEC_ID_RAWVIDEO: return "RAWVIDEO";
    case AV_CODEC_ID_MSMPEG4V1: return "MSMPEG4V1";
    case AV_CODEC_ID_MSMPEG4V2: return "MSMPEG4V2";
    case AV_CODEC_ID_MSMPEG4V3: return "MSMPEG4V3";
    case AV_CODEC_ID_WMV1: return "WMV1";
    case AV_CODEC_ID_WMV2: return "WMV2";
    case AV_CODEC_ID_H263P: return "H263P";
    case AV_CODEC_ID_H263I: return "H263I";
    case AV_CODEC_ID_FLV1: return "FLV1";
    case AV_CODEC_ID_SVQ1: return "SVQ1";
    case AV_CODEC_ID_SVQ3: return "SVQ3";
    case AV_CODEC_ID_DVVIDEO: return "DVVIDEO";
    case AV_CODEC_ID_HUFFYUV: return "HUFFYUV";
    case AV_CODEC_ID_CYUV: return "CYUV";
    case AV_CODEC_ID_H264: return "H264";
    case AV_CODEC_ID_INDEO3: return "INDEO3";
    case AV_CODEC_ID_VP3: return "VP3";
    case AV_CODEC_ID_THEORA: return "THEORA";
    case AV_CODEC_ID_ASV1: return "ASV1";
    case AV_CODEC_ID_ASV2: return "ASV2";
    case AV_CODEC_ID_FFV1: return "FFV1";
    case AV_CODEC_ID_4XM: return "4XM";
    case AV_CODEC_ID_VCR1: return "VCR1";
    case AV_CODEC_ID_CLJR: return "CLJR";
    case AV_CODEC_ID_MDEC: return "MDEC";
    case AV_CODEC_ID_ROQ: return "ROQ";
    case AV_CODEC_ID_INTERPLAY_VIDEO: return "INTERPLAY_VIDEO";
    case AV_CODEC_ID_XAN_WC3: return "XAN_WC3";
    case AV_CODEC_ID_XAN_WC4: return "XAN_WC4";
    case AV_CODEC_ID_RPZA: return "RPZA";
    case AV_CODEC_ID_CINEPAK: return "CINEPAK";
    case AV_CODEC_ID_WS_VQA: return "WS_VQA";
    case AV_CODEC_ID_MSRLE: return "MSRLE";
    case AV_CODEC_ID_MSVIDEO1: return "MSVIDEO1";
    case AV_CODEC_ID_IDCIN: return "IDCIN";
    case AV_CODEC_ID_8BPS: return "8BPS";
    case AV_CODEC_ID_SMC: return "SMC";
    case AV_CODEC_ID_FLIC: return "FLIC";
    case AV_CODEC_ID_TRUEMOTION1: return "TRUEMOTION1";
    case AV_CODEC_ID_VMDVIDEO: return "VMDVIDEO";
    case AV_CODEC_ID_MSZH: return "MSZH";
    case AV_CODEC_ID_ZLIB: return "ZLIB";
    case AV_CODEC_ID_QTRLE: return "QTRLE";
    case AV_CODEC_ID_SNOW: return "SNOW";
    case AV_CODEC_ID_TSCC: return "TSCC";
    case AV_CODEC_ID_ULTI: return "ULTI";
    case AV_CODEC_ID_QDRAW: return "QDRAW";
    case AV_CODEC_ID_VIXL: return "VIXL";
    case AV_CODEC_ID_QPEG: return "QPEG";
    case AV_CODEC_ID_PNG: return "PNG";
    case AV_CODEC_ID_PPM: return "PPM";
    case AV_CODEC_ID_PBM: return "PBM";
    case AV_CODEC_ID_PGM: return "PGM";
    case AV_CODEC_ID_PGMYUV: return "PGMYUV";
    case AV_CODEC_ID_PAM: return "PAM";
    case AV_CODEC_ID_FFVHUFF: return "FFVHUFF";
    case AV_CODEC_ID_RV30: return "RV30";
    case AV_CODEC_ID_RV40: return "RV40";
    case AV_CODEC_ID_VC1: return "VC1";
    case AV_CODEC_ID_WMV3: return "WMV3";
    case AV_CODEC_ID_LOCO: return "LOCO";
    case AV_CODEC_ID_WNV1: return "WNV1";
    case AV_CODEC_ID_AASC: return "AASC";
    case AV_CODEC_ID_INDEO2: return "INDEO2";
    case AV_CODEC_ID_FRAPS: return "FRAPS";
    case AV_CODEC_ID_TRUEMOTION2: return "TRUEMOTION2";
    case AV_CODEC_ID_BMP: return "BMP";
    case AV_CODEC_ID_CSCD: return "CSCD";
    case AV_CODEC_ID_MMVIDEO: return "MMVIDEO";
    case AV_CODEC_ID_ZMBV: return "ZMBV";
    case AV_CODEC_ID_AVS: return "AVS";
    case AV_CODEC_ID_SMACKVIDEO: return "SMACKVIDEO";
    case AV_CODEC_ID_NUV: return "NUV";
    case AV_CODEC_ID_KMVC: return "KMVC";
    case AV_CODEC_ID_FLASHSV: return "FLASHSV";
    case AV_CODEC_ID_CAVS: return "CAVS";
    case AV_CODEC_ID_JPEG2000: return "JPEG2000";
    case AV_CODEC_ID_VMNC: return "VMNC";
    case AV_CODEC_ID_VP5: return "VP5";
    case AV_CODEC_ID_VP6: return "VP6";
    case AV_CODEC_ID_VP6F: return "VP6F";
    case AV_CODEC_ID_TARGA: return "TARGA";
    case AV_CODEC_ID_DSICINVIDEO: return "DSICINVIDEO";
    case AV_CODEC_ID_TIERTEXSEQVIDEO: return "TIERTEXSEQVIDEO";
    case AV_CODEC_ID_TIFF: return "TIFF";
    case AV_CODEC_ID_GIF: return "GIF";
    //case CODEC_ID_FFH264: return "FFH264";
    case AV_CODEC_ID_DXA: return "DXA";
    case AV_CODEC_ID_DNXHD: return "DNXHD";
    case AV_CODEC_ID_THP: return "THP";
    case AV_CODEC_ID_SGI: return "SGI";
    case AV_CODEC_ID_C93: return "C93";
    case AV_CODEC_ID_BETHSOFTVID: return "BETHSOFTVID";
    case AV_CODEC_ID_PTX: return "PTX";
    case AV_CODEC_ID_TXD: return "TXD";
    case AV_CODEC_ID_VP6A: return "VP6A";
    case AV_CODEC_ID_AMV: return "AMV";
    case AV_CODEC_ID_VB: return "VB";
    case AV_CODEC_ID_PCX: return "PCX";
    case AV_CODEC_ID_SUNRAST: return "SUNRAST";
    case AV_CODEC_ID_INDEO4: return "INDEO4";
    case AV_CODEC_ID_INDEO5: return "INDEO5";
    case AV_CODEC_ID_MIMIC: return "MIMIC";
    case AV_CODEC_ID_RL2: return "RL2";
    case AV_CODEC_ID_8SVX_EXP: return "8SVX_EXP";
    case AV_CODEC_ID_8SVX_FIB: return "8SVX_FIB";
    case AV_CODEC_ID_ESCAPE124: return "ESCAPE124";
    case AV_CODEC_ID_DIRAC: return "DIRAC";
    case AV_CODEC_ID_BFI: return "BFI";
    case AV_CODEC_ID_CMV: return "CMV";
    case AV_CODEC_ID_MOTIONPIXELS: return "MOTIONPIXELS";
    case AV_CODEC_ID_TGV: return "TGV";
    case AV_CODEC_ID_TGQ: return "TGQ";
    case AV_CODEC_ID_TQI: return "TQI";
    case AV_CODEC_ID_AURA: return "AURA";
    case AV_CODEC_ID_AURA2: return "AURA2";
    case AV_CODEC_ID_V210X: return "V210X";
    case AV_CODEC_ID_TMV: return "TMV";
    case AV_CODEC_ID_V210: return "V210";
    case AV_CODEC_ID_DPX: return "DPX";
    case AV_CODEC_ID_MAD: return "MAD";
    case AV_CODEC_ID_FRWU: return "FRWU";
    case AV_CODEC_ID_FLASHSV2: return "FLASHSV2";
    case AV_CODEC_ID_CDGRAPHICS: return "CDGRAPHICS";
    case AV_CODEC_ID_R210: return "R210";
    case AV_CODEC_ID_ANM: return "ANM";
    case AV_CODEC_ID_BINKVIDEO: return "BINKVIDEO";
    case AV_CODEC_ID_IFF_ILBM: return "IFF_ILBM";
    case AV_CODEC_ID_IFF_BYTERUN1: return "BYTERUN1";
    case AV_CODEC_ID_KGV1: return "KGV1";
    case AV_CODEC_ID_YOP: return "YOP";
    case AV_CODEC_ID_VP8: return "VP8";
    case AV_CODEC_ID_PICTOR: return "PICTOR";
    case AV_CODEC_ID_ANSI: return "ANSI";
    case AV_CODEC_ID_A64_MULTI: return "A64_MULTI";
    case AV_CODEC_ID_A64_MULTI5: return "A64_MULTI5";
    case AV_CODEC_ID_R10K: return "R10K";
    case AV_CODEC_ID_MXPEG: return "MXPEG";
    case AV_CODEC_ID_LAGARITH: return "LAGARITH";
    case AV_CODEC_ID_PRORES: return "PRORES";
    case AV_CODEC_ID_JV: return "JV";
    case AV_CODEC_ID_DFA: return "DFA";
//    case AV_CODEC_ID_8SVX_RAW: return "8SVX_RAW";

        /* various PCM "codecs" */
    case AV_CODEC_ID_PCM_S16LE:
    case AV_CODEC_ID_PCM_S16BE:
    case AV_CODEC_ID_PCM_U16LE:
    case AV_CODEC_ID_PCM_U16BE:
    case AV_CODEC_ID_PCM_S8:
    case AV_CODEC_ID_PCM_U8:
    case AV_CODEC_ID_PCM_MULAW:
    case AV_CODEC_ID_PCM_ALAW:
    case AV_CODEC_ID_PCM_S32LE:
    case AV_CODEC_ID_PCM_S32BE:
    case AV_CODEC_ID_PCM_U32LE:
    case AV_CODEC_ID_PCM_U32BE:
    case AV_CODEC_ID_PCM_S24LE:
    case AV_CODEC_ID_PCM_S24BE:
    case AV_CODEC_ID_PCM_U24LE:
    case AV_CODEC_ID_PCM_U24BE:
    case AV_CODEC_ID_PCM_S24DAUD:
    case AV_CODEC_ID_PCM_ZORK:
    case AV_CODEC_ID_PCM_S16LE_PLANAR:
    case AV_CODEC_ID_PCM_DVD:
    case AV_CODEC_ID_PCM_F32BE:
    case AV_CODEC_ID_PCM_F32LE:
    case AV_CODEC_ID_PCM_F64BE:
    case AV_CODEC_ID_PCM_F64LE:
    case AV_CODEC_ID_PCM_BLURAY:
    case AV_CODEC_ID_PCM_LXF:
    case AV_CODEC_ID_S302M:

        /* various ADPCM codecs */
    case AV_CODEC_ID_ADPCM_IMA_QT:
    case AV_CODEC_ID_ADPCM_IMA_WAV:
    case AV_CODEC_ID_ADPCM_IMA_DK3:
    case AV_CODEC_ID_ADPCM_IMA_DK4:
    case AV_CODEC_ID_ADPCM_IMA_WS:
    case AV_CODEC_ID_ADPCM_IMA_SMJPEG:
    case AV_CODEC_ID_ADPCM_MS:
    case AV_CODEC_ID_ADPCM_4XM:
    case AV_CODEC_ID_ADPCM_XA:
    case AV_CODEC_ID_ADPCM_ADX:
    case AV_CODEC_ID_ADPCM_EA:
    case AV_CODEC_ID_ADPCM_G726:
    case AV_CODEC_ID_ADPCM_CT:
    case AV_CODEC_ID_ADPCM_SWF:
    case AV_CODEC_ID_ADPCM_YAMAHA:
    case AV_CODEC_ID_ADPCM_SBPRO_4:
    case AV_CODEC_ID_ADPCM_SBPRO_3:
    case AV_CODEC_ID_ADPCM_SBPRO_2:
    case AV_CODEC_ID_ADPCM_THP:
    case AV_CODEC_ID_ADPCM_IMA_AMV:
    case AV_CODEC_ID_ADPCM_EA_R1:
    case AV_CODEC_ID_ADPCM_EA_R3:
    case AV_CODEC_ID_ADPCM_EA_R2:
    case AV_CODEC_ID_ADPCM_IMA_EA_SEAD:
    case AV_CODEC_ID_ADPCM_IMA_EA_EACS:
    case AV_CODEC_ID_ADPCM_EA_XAS:
    case AV_CODEC_ID_ADPCM_EA_MAXIS_XA:
    case AV_CODEC_ID_ADPCM_IMA_ISS:
    case AV_CODEC_ID_ADPCM_G722:
        return "PCM";

        /* AMR */
    case AV_CODEC_ID_AMR_NB: return "AMR-NB";
    case AV_CODEC_ID_AMR_WB: return "AMR-WB";

        /* RealAudio codecs*/
    case AV_CODEC_ID_RA_144: return "RA-144";
    case AV_CODEC_ID_RA_288: return "RA-288";

        /* various DPCM codecs */
    case AV_CODEC_ID_ROQ_DPCM:
    case AV_CODEC_ID_INTERPLAY_DPCM:
    case AV_CODEC_ID_XAN_DPCM:
    case AV_CODEC_ID_SOL_DPCM:
        return "PCM";

        /* audio codecs */
    case AV_CODEC_ID_MP2: return "MP2";
    case AV_CODEC_ID_MP3: return "MP3"; ///< preferred ID for decoding MPEG audio layer 1: return ""; 2 or 3
    case AV_CODEC_ID_AAC: return "AAC";
    case AV_CODEC_ID_AC3: return "Dolby";
    case AV_CODEC_ID_DTS: return "DTS";
    case AV_CODEC_ID_VORBIS: return "VORBIS";
    case AV_CODEC_ID_DVAUDIO: return "DVAUDIO";
    case AV_CODEC_ID_WMAV1: return "WMAV1";
    case AV_CODEC_ID_WMAV2: return "WMAV2";
    case AV_CODEC_ID_MACE3: return "MACE3";
    case AV_CODEC_ID_MACE6: return "MACE6";
    case AV_CODEC_ID_VMDAUDIO: return "VMDAUDIO";
    case AV_CODEC_ID_SONIC: return "SONIC";
    case AV_CODEC_ID_SONIC_LS: return "SONIC_LC";
    case AV_CODEC_ID_FLAC: return "FLAC";
    case AV_CODEC_ID_MP3ADU: return "MP3";
    case AV_CODEC_ID_MP3ON4: return "MP3";
    case AV_CODEC_ID_SHORTEN: return "SHORTEN";
    case AV_CODEC_ID_ALAC: return "ALAC";
    case AV_CODEC_ID_WESTWOOD_SND1: return "WESTWOOD";
    case AV_CODEC_ID_GSM: return "GSM"; ///< as in Berlin toast format
    case AV_CODEC_ID_QDM2: return "QDM2";
    case AV_CODEC_ID_COOK: return "COOK";
    case AV_CODEC_ID_TRUESPEECH: return "TRUESPEECH";
    case AV_CODEC_ID_TTA: return "TTA";
    case AV_CODEC_ID_SMACKAUDIO: return "SMACKAUDIO";
    case AV_CODEC_ID_QCELP: return "QCELP";
    case AV_CODEC_ID_WAVPACK: return "WAVPACK";
    case AV_CODEC_ID_DSICINAUDIO: return "DSICINAUDIO";
    case AV_CODEC_ID_IMC: return "IMC";
    case AV_CODEC_ID_MUSEPACK7: return "MUSEPACK7";
    case AV_CODEC_ID_MLP: return "MLP";
    case AV_CODEC_ID_GSM_MS: return "GSM_MS"; /* as found in WAV */
    case AV_CODEC_ID_ATRAC3: return "ATRAC3";
    case AV_CODEC_ID_VOXWARE: return "VOXWARE";
    case AV_CODEC_ID_APE: return "APE";
    case AV_CODEC_ID_NELLYMOSER: return "NELLYMOSER";
    case AV_CODEC_ID_MUSEPACK8: return "MUSEPACK8";
    case AV_CODEC_ID_SPEEX: return "SPEEX";
    case AV_CODEC_ID_WMAVOICE: return "WMA VOICE";
    case AV_CODEC_ID_WMAPRO: return "WMA PRO";
    case AV_CODEC_ID_WMALOSSLESS: return "WMA LOSSLESS";
    case AV_CODEC_ID_ATRAC3P: return "ATRAC3P";
    case AV_CODEC_ID_EAC3: return "EAC3";
    case AV_CODEC_ID_SIPR: return "SIPR";
    case AV_CODEC_ID_MP1: return "MP1";
    case AV_CODEC_ID_TWINVQ: return "TWINVQ";
    case AV_CODEC_ID_TRUEHD: return "TRUE-HD";
    case AV_CODEC_ID_MP4ALS: return "MP4ALS";
    case AV_CODEC_ID_ATRAC1: return "ATRAC1";
    case AV_CODEC_ID_BINKAUDIO_RDFT: return "BINKAUDIO RDFT";
    case AV_CODEC_ID_BINKAUDIO_DCT: return "BINKAUDIO DCT";
    case AV_CODEC_ID_AAC_LATM: return "AAC LATM";
    case AV_CODEC_ID_QDMC: return "QDMC";
    case AV_CODEC_ID_CELT: return "CELT";

        /* subtitle codecs */
    case AV_CODEC_ID_DVD_SUBTITLE: return "DVD_SUBTITLE";
    case AV_CODEC_ID_DVB_SUBTITLE: return "DVB_SUBTITLE";
    case AV_CODEC_ID_TEXT: return "TEXT";  ///< raw UTF-8 text
    case AV_CODEC_ID_XSUB: return "XSUB";
    case AV_CODEC_ID_SSA: return "SSA";
    case AV_CODEC_ID_MOV_TEXT: return "MOV_TEXT";
    case AV_CODEC_ID_HDMV_PGS_SUBTITLE: return "HDMV_PGS_SUBTITLE";
    case AV_CODEC_ID_DVB_TELETEXT: return "DVB_TELETEXT";
    case AV_CODEC_ID_SRT: return "SRT";
    case AV_CODEC_ID_MICRODVD: return "MICRODVD";

        /* other specific kind of codecs (generally used for attachments) */
    case AV_CODEC_ID_TTF: return "TTF";

    case AV_CODEC_ID_PROBE: return "PROBE"; ///< codec_id is not known (like CODEC_ID_NONE) but lavf should attempt to identify it

    case AV_CODEC_ID_MPEG2TS: return "MPEG2TS"; /**< _FAKE_ codec to indicate a raw MPEG-2 TS
                                      * stream (only used by libavformat) */
    case AV_CODEC_ID_FFMETADATA: return "FFMETADATA";   ///< Dummy codec for streams containing only metadata information.

    case AV_CODEC_ID_NONE:
    default:
        break;
    };

    return 0;
}

QString codecIDToString(AVCodecID codecID)
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
    if (context && context->extradata_size > 0)
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

FrameTypeExtractor::FrameTypeExtractor(CodecID codecId, bool nalPrefixes)
:   m_context(nullptr),
    m_codecId(codecId),
    m_vcSequence(nullptr),
    m_dataWithNalPrefixes(nalPrefixes)
{
}

FrameTypeExtractor::~FrameTypeExtractor()
{
    delete m_vcSequence;
}

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
                /*int first_mb_in_slice =*/ NALUnit::extractUEGolombCode(bitReader);

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
    if (data[3] == VC1_CODE_FRAME || (data[3] == VC1_USER_CODE_FRAME && m_vcSequence))
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
        default:
            return UnknownFrameType;
    }
    
}

void QnFfmpegHelper::appendCtxField(QByteArray *dst, QnFfmpegHelper::CodecCtxField field, const char* data, int size)
{
    char f = (char) field;
    dst->append(&f, 1);
    int size1 = htonl(size);
    dst->append((const char*) &size1, 4);
    dst->append((const char*) data, size);
}

void QnFfmpegHelper::serializeCodecContext(const AVCodecContext *ctx, QByteArray *data)
{
    data->clear();
    data->append((const char*) &ctx->codec_id, 4);
    
    if (ctx->rc_eq) 
        appendCtxField(data, Field_RC_EQ, ctx->rc_eq, (int) strlen(ctx->rc_eq)+1); // +1 because of include \0 byte
    if (ctx->extradata) 
        appendCtxField(data, Field_EXTRADATA, (const char*) ctx->extradata, ctx->extradata_size);
    if (ctx->intra_matrix) 
        appendCtxField(data, Field_INTRA_MATRIX, (const char*) ctx->intra_matrix, 64 * sizeof(int16_t));
    if (ctx->inter_matrix)
        appendCtxField(data, Field_INTER_MATRIX, (const char*) ctx->inter_matrix, 64 * sizeof(int16_t));
    if (ctx->rc_override)
        appendCtxField(data, Field_OVERRIDE, (const char*) ctx->rc_override, ctx->rc_override_count * sizeof(*ctx->rc_override));

    if (ctx->channels > 0)
        appendCtxField(data, Field_Channels, (const char*) &ctx->channels, sizeof(int));
    if (ctx->sample_rate > 0)
        appendCtxField(data, Field_SampleRate, (const char*) &ctx->sample_rate, sizeof(int));
    if (ctx->sample_fmt != AV_SAMPLE_FMT_NONE)
        appendCtxField(data, Field_Sample_Fmt, (const char*) &ctx->sample_fmt, sizeof(AVSampleFormat));
    if (ctx->bits_per_coded_sample > 0)
        appendCtxField(data, Field_BitsPerSample, (const char*) &ctx->bits_per_coded_sample, sizeof(int));
    if (ctx->coded_width > 0)
        appendCtxField(data, Field_CodedWidth, (const char*) &ctx->coded_width, sizeof(int));
    if (ctx->coded_height > 0)
        appendCtxField(data, Field_CodedHeight, (const char*) &ctx->coded_height, sizeof(int));
}

AVCodecContext *QnFfmpegHelper::deserializeCodecContext(const char *data, int dataLen)
{
    AVCodec* codec = 0;

    QByteArray tmpArray(data, dataLen);
    QBuffer buffer(&tmpArray);
    buffer.open(QIODevice::ReadOnly);
    AVCodecContext* ctx = 0;
    AVCodecID codecId = CODEC_ID_NONE;
    int readed = buffer.read((char*) &codecId, 4);
    if (readed < 4)
        goto error_label;
    codec = avcodec_find_decoder(codecId);

    ctx = (AVCodecContext*) avcodec_alloc_context3(codec);

    if (codec == 0)
        goto error_label;
    
    char objectType;
    
    while (1)
    {
        int readed = buffer.read(&objectType, 1);
        if (readed < 1)
            break;
        CodecCtxField field = (CodecCtxField) objectType;
        int size;
        if (buffer.read((char*) &size, 4) != 4)
            goto error_label;
        size = ntohl(size);
        char* fieldData = (char*) av_malloc(size);
        readed = buffer.read(fieldData, size);
        if (readed != size) {
            av_free(fieldData);
            goto error_label;
        }

        switch (field)
        {
            case Field_RC_EQ:
                ctx->rc_eq = fieldData;
                break;
            case Field_EXTRADATA:
                ctx->extradata = (quint8*) fieldData;
                ctx->extradata_size = size;
                break;
            case Field_INTRA_MATRIX:
                ctx->intra_matrix = (quint16*) fieldData;
                break;
            case Field_INTER_MATRIX: 
                ctx->inter_matrix = (quint16*) fieldData;
                break;
            case Field_OVERRIDE:
                ctx->rc_override = (RcOverride*) fieldData;
                ctx->rc_override_count = size / sizeof(*ctx->rc_override);
                break;
            case Field_Channels:
                ctx->channels = *(int*) fieldData;
                av_free(fieldData);
                break;
            case Field_SampleRate:
                ctx->sample_rate = *(int*) fieldData;
                av_free(fieldData);
                break;
            case Field_Sample_Fmt:
                ctx->sample_fmt = *(AVSampleFormat*) fieldData;
                av_free(fieldData);
                break;
            case Field_BitsPerSample:
                ctx->bits_per_coded_sample = *(int*) fieldData;
                av_free(fieldData);
                break;
            case Field_CodedWidth:
                ctx->coded_width = *(int*) fieldData;
                av_free(fieldData);
                break;
            case Field_CodedHeight:
                ctx->coded_height = *(int*) fieldData;
                av_free(fieldData);
                break;
        }
    }

    ctx->codec_id = codecId;
	ctx->codec_type = codec->type;
    return ctx;

error_label:
    qWarning() << Q_FUNC_INFO << __LINE__ << "Parse error in deserialize CodecContext";
    if (ctx)
        QnFfmpegHelper::deleteCodecContext(ctx);
    return 0;
}

QString getAudioCodecDescription(AVCodecContext* codecContext)
{
    QString result;
    QString codecStr = codecIDToString(codecContext->codec_id);
    if (!codecStr.isEmpty())
    {
        result += codecStr;
        result += QLatin1Char(' ');
    }

    //str += QString::number(codecContext->sample_rate / 1000)+ QLatin1String("Khz ");
    if (codecContext->channels == 3)
        result += QLatin1String("2.1");
    else if (codecContext->channels == 6)
        result += QLatin1String("5.1");
    else if (codecContext->channels == 8)
        result += QLatin1String("7.1");
    else if (codecContext->channels == 2)
        result += QLatin1String("stereo");
    else if (codecContext->channels == 1)
        result += QLatin1String("mono");
    else
        result += QString::number(codecContext->channels);

    return result;
}

static qint32 ffmpegReadPacket(void *opaque, quint8* buf, int size)
{
    QIODevice* reader = reinterpret_cast<QIODevice*> (opaque);
    return reader->read((char*) buf, size);
}

static qint32 ffmpegWritePacket(void *opaque, quint8* buf, int size)
{
    QIODevice* reader = reinterpret_cast<QIODevice*> (opaque);
    if (reader)
        return reader->write((char*) buf, size);
    else
        return 0;
}

static int64_t ffmpegSeek(void* opaque, int64_t pos, int whence)
{
    QIODevice* reader = reinterpret_cast<QIODevice*> (opaque);

    qint64 absolutePos = pos;
    switch (whence)
    {
    case SEEK_SET: 
        break;
    case SEEK_CUR: 
        absolutePos = reader->pos() + pos;
        break;
    case SEEK_END: 
        absolutePos = reader->size() + pos;
        break;
    case 65536:
        return reader->size();
    default:
        return AVERROR(ENOENT);
    }

    return reader->seek(absolutePos);
}


AVIOContext* QnFfmpegHelper::createFfmpegIOContext(QnStorageResourcePtr resource, const QString& url, QIODevice::OpenMode openMode, int ioBlockSize)
{
    int prefix = url.indexOf(QLatin1String("://"));
    QString path = prefix == -1 ? url : url.mid(prefix+3);

    quint8* ioBuffer;
    AVIOContext* ffmpegIOContext;

    QIODevice* ioDevice = resource->open(path, openMode);
    if (ioDevice == 0)
        return 0;

    ioBuffer = (quint8*) av_malloc(ioBlockSize);
    ffmpegIOContext = avio_alloc_context(
        ioBuffer,
        ioBlockSize,
        (openMode & QIODevice::WriteOnly) ? 1 : 0,
        ioDevice,
        &ffmpegReadPacket,
        &ffmpegWritePacket,
        &ffmpegSeek);

    return ffmpegIOContext;
}

AVIOContext* QnFfmpegHelper::createFfmpegIOContext(QIODevice* ioDevice, int ioBlockSize)
{
    quint8* ioBuffer;
    AVIOContext* ffmpegIOContext;

    ioBuffer = (quint8*) av_malloc(ioBlockSize);
    ffmpegIOContext = avio_alloc_context(
        ioBuffer,
        ioBlockSize,
        (ioDevice->openMode() & QIODevice::WriteOnly) ? 1 : 0,
        ioDevice,
        &ffmpegReadPacket,
        &ffmpegWritePacket,
        &ffmpegSeek);

    return ffmpegIOContext;
}

qint64 QnFfmpegHelper::getFileSizeByIOContext(AVIOContext* ioContext)
{
    if (ioContext)
    {
        QIODevice* ioDevice = (QIODevice*) ioContext->opaque;
        return ioDevice->size();
    }
    return 0;
}

void QnFfmpegHelper::closeFfmpegIOContext(AVIOContext* ioContext)
{
    if (ioContext)
    {
        QIODevice* ioDevice = (QIODevice*) ioContext->opaque;
        delete ioDevice;
        ioContext->opaque = 0;
        avio_close(ioContext);
    }
}

void QnFfmpegHelper::deleteCodecContext(AVCodecContext* ctx)
{
    if (!ctx)
        return;
    avcodec_close(ctx);
    av_freep(&ctx->rc_override);
    av_freep(&ctx->intra_matrix);
    av_freep(&ctx->inter_matrix);
    av_freep(&ctx->extradata);
    av_freep(&ctx->rc_eq);
    av_freep(&ctx);
}

#endif // ENABLE_DATA_PROVIDERS
