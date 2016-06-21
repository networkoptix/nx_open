#include "av_codec_helper.h"

#include <nx/utils/log/assert.h>

static const char* codecIdToLiteral(AVCodecID codecId)
{
    switch (codecId)
    {
        /* video codecs */
        case AV_CODEC_ID_MPEG1VIDEO: return "MPEG1VIDEO";
        case AV_CODEC_ID_MPEG2VIDEO: return "MPEG2VIDEO"; //< Preferred id for MPEG-1/2 video decoding.
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
        //case AV_CODEC_ID_FFH264: return "FFH264";
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
        // case AV_CODEC_ID_IFF_BYTERUN1: return "BYTERUN1";
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
        // case AV_CODEC_ID_8SVX_RAW: return "8SVX_RAW";

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
        case AV_CODEC_ID_MP3: return "MP3"; //< Preferred id for decoding MPEG audio layer 1: return ""; 2 or 3.
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
        case AV_CODEC_ID_GSM: return "GSM"; //< As in Berlin toast format.
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
        case AV_CODEC_ID_GSM_MS: return "GSM_MS"; //< As found in WAV.
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
        case AV_CODEC_ID_TEXT: return "TEXT"; //< Raw UTF-8 text.
        case AV_CODEC_ID_XSUB: return "XSUB";
        case AV_CODEC_ID_SSA: return "SSA";
        case AV_CODEC_ID_MOV_TEXT: return "MOV_TEXT";
        case AV_CODEC_ID_HDMV_PGS_SUBTITLE: return "HDMV_PGS_SUBTITLE";
        case AV_CODEC_ID_DVB_TELETEXT: return "DVB_TELETEXT";
        case AV_CODEC_ID_SRT: return "SRT";
        case AV_CODEC_ID_MICRODVD: return "MICRODVD";

        /* other specific kind of codecs (generally used for attachments) */
        case AV_CODEC_ID_TTF: return "TTF";

        // codec_id is not known (like AV_CODEC_ID_NONE) but lavf should attempt to
        // identify it.
        case AV_CODEC_ID_PROBE: return "PROBE";

        // _FAKE_ codec to indicate a raw MPEG-2 TS stream (only used by 
        // libavformat).
        case AV_CODEC_ID_MPEG2TS: return "MPEG2TS";

        // Dummy codec for streams containing only metadata information.
        case AV_CODEC_ID_FFMETADATA: return "FFMETADATA";

        case AV_CODEC_ID_NONE: return "";

        default:
            NX_ASSERT(false, Q_FUNC_INFO, "Unregistered enum AVCodecID value.");
            return "";
    }
}

QString QnAvCodecHelper::codecIdToString(AVCodecID codecId)
{
    return QString::fromLatin1(codecIdToLiteral(codecId));
}
