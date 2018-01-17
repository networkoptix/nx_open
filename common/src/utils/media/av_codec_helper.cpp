#include "av_codec_helper.h"
#include <nx/utils/log/assert.h>

#include <array>
#include <QtCore/QByteArray>
#include <nx/utils/thread/mutex.h>

QMap<AVCodecID, QByteArray> codecToStringData()
{
    static QnMutex mutex;
    QnMutexLocker lock(&mutex);

    static QMap<AVCodecID, QByteArray> codecToStringData;
    if (!codecToStringData.isEmpty())
        return codecToStringData;

    /* video codecs */
    codecToStringData.insert(AV_CODEC_ID_MPEG1VIDEO, "MPEG1VIDEO");
    codecToStringData.insert(AV_CODEC_ID_MPEG2VIDEO, "MPEG2VIDEO"); //< Preferred id for MPEG-1/2 video decoding.
    codecToStringData.insert(AV_CODEC_ID_MPEG2VIDEO_XVMC, "MPEG2VIDEO-XVMC");
    codecToStringData.insert(AV_CODEC_ID_H261, "H261");
    codecToStringData.insert(AV_CODEC_ID_H263, "H263");
    codecToStringData.insert(AV_CODEC_ID_RV10, "RV10");
    codecToStringData.insert(AV_CODEC_ID_RV20, "RV20");
    codecToStringData.insert(AV_CODEC_ID_MJPEG, "MJPEG");
    codecToStringData.insert(AV_CODEC_ID_MJPEGB, "MJPEGB");
    codecToStringData.insert(AV_CODEC_ID_LJPEG, "LJPEG");
    codecToStringData.insert(AV_CODEC_ID_SP5X, "SP5X");
    codecToStringData.insert(AV_CODEC_ID_JPEGLS, "JPEGLS");
    codecToStringData.insert(AV_CODEC_ID_MPEG4, "MPEG4");
    codecToStringData.insert(AV_CODEC_ID_RAWVIDEO, "RAWVIDEO");
    codecToStringData.insert(AV_CODEC_ID_MSMPEG4V1, "MSMPEG4V1");
    codecToStringData.insert(AV_CODEC_ID_MSMPEG4V2, "MSMPEG4V2");
    codecToStringData.insert(AV_CODEC_ID_MSMPEG4V3, "MSMPEG4V3");
    codecToStringData.insert(AV_CODEC_ID_WMV1, "WMV1");
    codecToStringData.insert(AV_CODEC_ID_WMV2, "WMV2");
    codecToStringData.insert(AV_CODEC_ID_H263P, "H263P");
    codecToStringData.insert(AV_CODEC_ID_H263I, "H263I");
    codecToStringData.insert(AV_CODEC_ID_FLV1, "FLV1");
    codecToStringData.insert(AV_CODEC_ID_SVQ1, "SVQ1");
    codecToStringData.insert(AV_CODEC_ID_SVQ3, "SVQ3");
    codecToStringData.insert(AV_CODEC_ID_DVVIDEO, "DVVIDEO");
    codecToStringData.insert(AV_CODEC_ID_HUFFYUV, "HUFFYUV");
    codecToStringData.insert(AV_CODEC_ID_CYUV, "CYUV");
    codecToStringData.insert(AV_CODEC_ID_H264, "H264");
    codecToStringData.insert(AV_CODEC_ID_HEVC, "H265");
    codecToStringData.insert(AV_CODEC_ID_INDEO3, "INDEO3");
    codecToStringData.insert(AV_CODEC_ID_VP3, "VP3");
    codecToStringData.insert(AV_CODEC_ID_THEORA, "THEORA");
    codecToStringData.insert(AV_CODEC_ID_ASV1, "ASV1");
    codecToStringData.insert(AV_CODEC_ID_ASV2, "ASV2");
    codecToStringData.insert(AV_CODEC_ID_FFV1, "FFV1");
    codecToStringData.insert(AV_CODEC_ID_4XM, "4XM");
    codecToStringData.insert(AV_CODEC_ID_VCR1, "VCR1");
    codecToStringData.insert(AV_CODEC_ID_CLJR, "CLJR");
    codecToStringData.insert(AV_CODEC_ID_MDEC, "MDEC");
    codecToStringData.insert(AV_CODEC_ID_ROQ, "ROQ");
    codecToStringData.insert(AV_CODEC_ID_INTERPLAY_VIDEO, "INTERPLAY_VIDEO");
    codecToStringData.insert(AV_CODEC_ID_XAN_WC3, "XAN_WC3");
    codecToStringData.insert(AV_CODEC_ID_XAN_WC4, "XAN_WC4");
    codecToStringData.insert(AV_CODEC_ID_RPZA, "RPZA");
    codecToStringData.insert(AV_CODEC_ID_CINEPAK, "CINEPAK");
    codecToStringData.insert(AV_CODEC_ID_WS_VQA, "WS_VQA");
    codecToStringData.insert(AV_CODEC_ID_MSRLE, "MSRLE");
    codecToStringData.insert(AV_CODEC_ID_MSVIDEO1, "MSVIDEO1");
    codecToStringData.insert(AV_CODEC_ID_IDCIN, "IDCIN");
    codecToStringData.insert(AV_CODEC_ID_8BPS, "8BPS");
    codecToStringData.insert(AV_CODEC_ID_SMC, "SMC");
    codecToStringData.insert(AV_CODEC_ID_FLIC, "FLIC");
    codecToStringData.insert(AV_CODEC_ID_TRUEMOTION1, "TRUEMOTION1");
    codecToStringData.insert(AV_CODEC_ID_VMDVIDEO, "VMDVIDEO");
    codecToStringData.insert(AV_CODEC_ID_MSZH, "MSZH");
    codecToStringData.insert(AV_CODEC_ID_ZLIB, "ZLIB");
    codecToStringData.insert(AV_CODEC_ID_QTRLE, "QTRLE");
    codecToStringData.insert(AV_CODEC_ID_SNOW, "SNOW");
    codecToStringData.insert(AV_CODEC_ID_TSCC, "TSCC");
    codecToStringData.insert(AV_CODEC_ID_ULTI, "ULTI");
    codecToStringData.insert(AV_CODEC_ID_QDRAW, "QDRAW");
    codecToStringData.insert(AV_CODEC_ID_VIXL, "VIXL");
    codecToStringData.insert(AV_CODEC_ID_QPEG, "QPEG");
    codecToStringData.insert(AV_CODEC_ID_PNG, "PNG");
    codecToStringData.insert(AV_CODEC_ID_PPM, "PPM");
    codecToStringData.insert(AV_CODEC_ID_PBM, "PBM");
    codecToStringData.insert(AV_CODEC_ID_PGM, "PGM");
    codecToStringData.insert(AV_CODEC_ID_PGMYUV, "PGMYUV");
    codecToStringData.insert(AV_CODEC_ID_PAM, "PAM");
    codecToStringData.insert(AV_CODEC_ID_FFVHUFF, "FFVHUFF");
    codecToStringData.insert(AV_CODEC_ID_RV30, "RV30");
    codecToStringData.insert(AV_CODEC_ID_RV40, "RV40");
    codecToStringData.insert(AV_CODEC_ID_VC1, "VC1");
    codecToStringData.insert(AV_CODEC_ID_WMV3, "WMV3");
    codecToStringData.insert(AV_CODEC_ID_LOCO, "LOCO");
    codecToStringData.insert(AV_CODEC_ID_WNV1, "WNV1");
    codecToStringData.insert(AV_CODEC_ID_AASC, "AASC");
    codecToStringData.insert(AV_CODEC_ID_INDEO2, "INDEO2");
    codecToStringData.insert(AV_CODEC_ID_FRAPS, "FRAPS");
    codecToStringData.insert(AV_CODEC_ID_TRUEMOTION2, "TRUEMOTION2");
    codecToStringData.insert(AV_CODEC_ID_BMP, "BMP");
    codecToStringData.insert(AV_CODEC_ID_CSCD, "CSCD");
    codecToStringData.insert(AV_CODEC_ID_MMVIDEO, "MMVIDEO");
    codecToStringData.insert(AV_CODEC_ID_ZMBV, "ZMBV");
    codecToStringData.insert(AV_CODEC_ID_AVS, "AVS");
    codecToStringData.insert(AV_CODEC_ID_SMACKVIDEO, "SMACKVIDEO");
    codecToStringData.insert(AV_CODEC_ID_NUV, "NUV");
    codecToStringData.insert(AV_CODEC_ID_KMVC, "KMVC");
    codecToStringData.insert(AV_CODEC_ID_FLASHSV, "FLASHSV");
    codecToStringData.insert(AV_CODEC_ID_CAVS, "CAVS");
    codecToStringData.insert(AV_CODEC_ID_JPEG2000, "JPEG2000");
    codecToStringData.insert(AV_CODEC_ID_VMNC, "VMNC");
    codecToStringData.insert(AV_CODEC_ID_VP5, "VP5");
    codecToStringData.insert(AV_CODEC_ID_VP6, "VP6");
    codecToStringData.insert(AV_CODEC_ID_VP6F, "VP6F");
    codecToStringData.insert(AV_CODEC_ID_TARGA, "TARGA");
    codecToStringData.insert(AV_CODEC_ID_DSICINVIDEO, "DSICINVIDEO");
    codecToStringData.insert(AV_CODEC_ID_TIERTEXSEQVIDEO, "TIERTEXSEQVIDEO");
    codecToStringData.insert(AV_CODEC_ID_TIFF, "TIFF");
    codecToStringData.insert(AV_CODEC_ID_GIF, "GIF");
    //codecToStringData.insert(AV_CODEC_ID_FFH264,  "FFH264");
    codecToStringData.insert(AV_CODEC_ID_DXA, "DXA");
    codecToStringData.insert(AV_CODEC_ID_DNXHD, "DNXHD");
    codecToStringData.insert(AV_CODEC_ID_THP, "THP");
    codecToStringData.insert(AV_CODEC_ID_SGI, "SGI");
    codecToStringData.insert(AV_CODEC_ID_C93, "C93");
    codecToStringData.insert(AV_CODEC_ID_BETHSOFTVID, "BETHSOFTVID");
    codecToStringData.insert(AV_CODEC_ID_PTX, "PTX");
    codecToStringData.insert(AV_CODEC_ID_TXD, "TXD");
    codecToStringData.insert(AV_CODEC_ID_VP6A, "VP6A");
    codecToStringData.insert(AV_CODEC_ID_AMV, "AMV");
    codecToStringData.insert(AV_CODEC_ID_VB, "VB");
    codecToStringData.insert(AV_CODEC_ID_PCX, "PCX");
    codecToStringData.insert(AV_CODEC_ID_SUNRAST, "SUNRAST");
    codecToStringData.insert(AV_CODEC_ID_INDEO4, "INDEO4");
    codecToStringData.insert(AV_CODEC_ID_INDEO5, "INDEO5");
    codecToStringData.insert(AV_CODEC_ID_MIMIC, "MIMIC");
    codecToStringData.insert(AV_CODEC_ID_RL2, "RL2");
    codecToStringData.insert(AV_CODEC_ID_8SVX_EXP, "8SVX_EXP");
    codecToStringData.insert(AV_CODEC_ID_8SVX_FIB, "8SVX_FIB");
    codecToStringData.insert(AV_CODEC_ID_ESCAPE124, "ESCAPE124");
    codecToStringData.insert(AV_CODEC_ID_DIRAC, "DIRAC");
    codecToStringData.insert(AV_CODEC_ID_BFI, "BFI");
    codecToStringData.insert(AV_CODEC_ID_CMV, "CMV");
    codecToStringData.insert(AV_CODEC_ID_MOTIONPIXELS, "MOTIONPIXELS");
    codecToStringData.insert(AV_CODEC_ID_TGV, "TGV");
    codecToStringData.insert(AV_CODEC_ID_TGQ, "TGQ");
    codecToStringData.insert(AV_CODEC_ID_TQI, "TQI");
    codecToStringData.insert(AV_CODEC_ID_AURA, "AURA");
    codecToStringData.insert(AV_CODEC_ID_AURA2, "AURA2");
    codecToStringData.insert(AV_CODEC_ID_V210X, "V210X");
    codecToStringData.insert(AV_CODEC_ID_TMV, "TMV");
    codecToStringData.insert(AV_CODEC_ID_V210, "V210");
    codecToStringData.insert(AV_CODEC_ID_DPX, "DPX");
    codecToStringData.insert(AV_CODEC_ID_MAD, "MAD");
    codecToStringData.insert(AV_CODEC_ID_FRWU, "FRWU");
    codecToStringData.insert(AV_CODEC_ID_FLASHSV2, "FLASHSV2");
    codecToStringData.insert(AV_CODEC_ID_CDGRAPHICS, "CDGRAPHICS");
    codecToStringData.insert(AV_CODEC_ID_R210, "R210");
    codecToStringData.insert(AV_CODEC_ID_ANM, "ANM");
    codecToStringData.insert(AV_CODEC_ID_BINKVIDEO, "BINKVIDEO");
    codecToStringData.insert(AV_CODEC_ID_IFF_ILBM, "IFF_ILBM");
    // codecToStringData.insert(AV_CODEC_ID_IFF_BYTERUN1,  "BYTERUN1");
    codecToStringData.insert(AV_CODEC_ID_KGV1, "KGV1");
    codecToStringData.insert(AV_CODEC_ID_YOP, "YOP");
    codecToStringData.insert(AV_CODEC_ID_VP8, "VP8");
    codecToStringData.insert(AV_CODEC_ID_PICTOR, "PICTOR");
    codecToStringData.insert(AV_CODEC_ID_ANSI, "ANSI");
    codecToStringData.insert(AV_CODEC_ID_A64_MULTI, "A64_MULTI");
    codecToStringData.insert(AV_CODEC_ID_A64_MULTI5, "A64_MULTI5");
    codecToStringData.insert(AV_CODEC_ID_R10K, "R10K");
    codecToStringData.insert(AV_CODEC_ID_MXPEG, "MXPEG");
    codecToStringData.insert(AV_CODEC_ID_LAGARITH, "LAGARITH");
    codecToStringData.insert(AV_CODEC_ID_PRORES, "PRORES");
    codecToStringData.insert(AV_CODEC_ID_JV, "JV");
    codecToStringData.insert(AV_CODEC_ID_DFA, "DFA");
    // codecToStringData.insert(AV_CODEC_ID_8SVX_RAW,  "8SVX_RAW");

    /* various PCM "codecs" */
    codecToStringData.insert(AV_CODEC_ID_PCM_S16LE, "PCM");
    codecToStringData.insert(AV_CODEC_ID_PCM_S16BE, "PCM");
    codecToStringData.insert(AV_CODEC_ID_PCM_U16LE, "PCM");
    codecToStringData.insert(AV_CODEC_ID_PCM_U16BE, "PCM");
    codecToStringData.insert(AV_CODEC_ID_PCM_S8, "PCM");
    codecToStringData.insert(AV_CODEC_ID_PCM_U8, "PCM");
    codecToStringData.insert(AV_CODEC_ID_PCM_MULAW, "PCM");
    codecToStringData.insert(AV_CODEC_ID_PCM_ALAW, "PCM");
    codecToStringData.insert(AV_CODEC_ID_PCM_S32LE, "PCM");
    codecToStringData.insert(AV_CODEC_ID_PCM_S32BE, "PCM");
    codecToStringData.insert(AV_CODEC_ID_PCM_U32LE, "PCM");
    codecToStringData.insert(AV_CODEC_ID_PCM_U32BE, "PCM");
    codecToStringData.insert(AV_CODEC_ID_PCM_S24LE, "PCM");
    codecToStringData.insert(AV_CODEC_ID_PCM_S24BE, "PCM");
    codecToStringData.insert(AV_CODEC_ID_PCM_U24LE, "PCM");
    codecToStringData.insert(AV_CODEC_ID_PCM_U24BE, "PCM");
    codecToStringData.insert(AV_CODEC_ID_PCM_S24DAUD, "PCM");
    codecToStringData.insert(AV_CODEC_ID_PCM_ZORK, "PCM");
    codecToStringData.insert(AV_CODEC_ID_PCM_S16LE_PLANAR, "PCM");
    codecToStringData.insert(AV_CODEC_ID_PCM_DVD, "PCM");
    codecToStringData.insert(AV_CODEC_ID_PCM_F32BE, "PCM");
    codecToStringData.insert(AV_CODEC_ID_PCM_F32LE, "PCM");
    codecToStringData.insert(AV_CODEC_ID_PCM_F64BE, "PCM");
    codecToStringData.insert(AV_CODEC_ID_PCM_F64LE, "PCM");
    codecToStringData.insert(AV_CODEC_ID_PCM_BLURAY, "PCM");
    codecToStringData.insert(AV_CODEC_ID_PCM_LXF, "PCM");
    codecToStringData.insert(AV_CODEC_ID_S302M, "PCM");

    /* various ADPCM codecs */
    codecToStringData.insert(AV_CODEC_ID_ADPCM_IMA_QT, "PCM");
    codecToStringData.insert(AV_CODEC_ID_ADPCM_IMA_WAV, "PCM");
    codecToStringData.insert(AV_CODEC_ID_ADPCM_IMA_DK3, "PCM");
    codecToStringData.insert(AV_CODEC_ID_ADPCM_IMA_DK4, "PCM");
    codecToStringData.insert(AV_CODEC_ID_ADPCM_IMA_WS, "PCM");
    codecToStringData.insert(AV_CODEC_ID_ADPCM_IMA_SMJPEG, "PCM");
    codecToStringData.insert(AV_CODEC_ID_ADPCM_MS, "PCM");
    codecToStringData.insert(AV_CODEC_ID_ADPCM_4XM, "PCM");
    codecToStringData.insert(AV_CODEC_ID_ADPCM_XA, "PCM");
    codecToStringData.insert(AV_CODEC_ID_ADPCM_ADX, "PCM");
    codecToStringData.insert(AV_CODEC_ID_ADPCM_EA, "PCM");
    codecToStringData.insert(AV_CODEC_ID_ADPCM_G726, "PCM");
    codecToStringData.insert(AV_CODEC_ID_ADPCM_CT, "PCM");
    codecToStringData.insert(AV_CODEC_ID_ADPCM_SWF, "PCM");
    codecToStringData.insert(AV_CODEC_ID_ADPCM_YAMAHA, "PCM");
    codecToStringData.insert(AV_CODEC_ID_ADPCM_SBPRO_4, "PCM");
    codecToStringData.insert(AV_CODEC_ID_ADPCM_SBPRO_3, "PCM");
    codecToStringData.insert(AV_CODEC_ID_ADPCM_SBPRO_2, "PCM");
    codecToStringData.insert(AV_CODEC_ID_ADPCM_THP, "PCM");
    codecToStringData.insert(AV_CODEC_ID_ADPCM_IMA_AMV, "PCM");
    codecToStringData.insert(AV_CODEC_ID_ADPCM_EA_R1, "PCM");
    codecToStringData.insert(AV_CODEC_ID_ADPCM_EA_R3, "PCM");
    codecToStringData.insert(AV_CODEC_ID_ADPCM_EA_R2, "PCM");
    codecToStringData.insert(AV_CODEC_ID_ADPCM_IMA_EA_SEAD, "PCM");
    codecToStringData.insert(AV_CODEC_ID_ADPCM_IMA_EA_EACS, "PCM");
    codecToStringData.insert(AV_CODEC_ID_ADPCM_EA_XAS, "PCM");
    codecToStringData.insert(AV_CODEC_ID_ADPCM_EA_MAXIS_XA, "PCM");
    codecToStringData.insert(AV_CODEC_ID_ADPCM_IMA_ISS, "PCM");
    codecToStringData.insert(AV_CODEC_ID_ADPCM_G722, "PCM");

    /* AMR */
    codecToStringData.insert(AV_CODEC_ID_AMR_NB, "AMR-NB");
    codecToStringData.insert(AV_CODEC_ID_AMR_WB, "AMR-WB");

    /* RealAudio codecs*/
    codecToStringData.insert(AV_CODEC_ID_RA_144, "RA-144");
    codecToStringData.insert(AV_CODEC_ID_RA_288, "RA-288");

    /* various DPCM codecs */
    codecToStringData.insert(AV_CODEC_ID_ROQ_DPCM, "PCM");
    codecToStringData.insert(AV_CODEC_ID_INTERPLAY_DPCM, "PCM");
    codecToStringData.insert(AV_CODEC_ID_XAN_DPCM, "PCM");
    codecToStringData.insert(AV_CODEC_ID_SOL_DPCM, "PCM");

    /* audio codecs */
    codecToStringData.insert(AV_CODEC_ID_MP2, "MP2");
    codecToStringData.insert(AV_CODEC_ID_MP3, "MP3"); //< Preferred id for decoding MPEG audio layer 1,  ""); 2 or 3.
    codecToStringData.insert(AV_CODEC_ID_AAC, "AAC");
    codecToStringData.insert(AV_CODEC_ID_AC3, "Dolby");
    codecToStringData.insert(AV_CODEC_ID_DTS, "DTS");
    codecToStringData.insert(AV_CODEC_ID_VORBIS, "VORBIS");
    codecToStringData.insert(AV_CODEC_ID_DVAUDIO, "DVAUDIO");
    codecToStringData.insert(AV_CODEC_ID_WMAV1, "WMAV1");
    codecToStringData.insert(AV_CODEC_ID_WMAV2, "WMAV2");
    codecToStringData.insert(AV_CODEC_ID_MACE3, "MACE3");
    codecToStringData.insert(AV_CODEC_ID_MACE6, "MACE6");
    codecToStringData.insert(AV_CODEC_ID_VMDAUDIO, "VMDAUDIO");
    codecToStringData.insert(AV_CODEC_ID_SONIC, "SONIC");
    codecToStringData.insert(AV_CODEC_ID_SONIC_LS, "SONIC_LC");
    codecToStringData.insert(AV_CODEC_ID_FLAC, "FLAC");
    codecToStringData.insert(AV_CODEC_ID_MP3ADU, "MP3");
    codecToStringData.insert(AV_CODEC_ID_MP3ON4, "MP3");
    codecToStringData.insert(AV_CODEC_ID_SHORTEN, "SHORTEN");
    codecToStringData.insert(AV_CODEC_ID_ALAC, "ALAC");
    codecToStringData.insert(AV_CODEC_ID_WESTWOOD_SND1, "WESTWOOD");
    codecToStringData.insert(AV_CODEC_ID_GSM, "GSM"); //< As in Berlin toast format.
    codecToStringData.insert(AV_CODEC_ID_QDM2, "QDM2");
    codecToStringData.insert(AV_CODEC_ID_COOK, "COOK");
    codecToStringData.insert(AV_CODEC_ID_TRUESPEECH, "TRUESPEECH");
    codecToStringData.insert(AV_CODEC_ID_TTA, "TTA");
    codecToStringData.insert(AV_CODEC_ID_SMACKAUDIO, "SMACKAUDIO");
    codecToStringData.insert(AV_CODEC_ID_QCELP, "QCELP");
    codecToStringData.insert(AV_CODEC_ID_WAVPACK, "WAVPACK");
    codecToStringData.insert(AV_CODEC_ID_DSICINAUDIO, "DSICINAUDIO");
    codecToStringData.insert(AV_CODEC_ID_IMC, "IMC");
    codecToStringData.insert(AV_CODEC_ID_MUSEPACK7, "MUSEPACK7");
    codecToStringData.insert(AV_CODEC_ID_MLP, "MLP");
    codecToStringData.insert(AV_CODEC_ID_GSM_MS, "GSM_MS"); //< As found in WAV.
    codecToStringData.insert(AV_CODEC_ID_ATRAC3, "ATRAC3");
    codecToStringData.insert(AV_CODEC_ID_VOXWARE, "VOXWARE");
    codecToStringData.insert(AV_CODEC_ID_APE, "APE");
    codecToStringData.insert(AV_CODEC_ID_NELLYMOSER, "NELLYMOSER");
    codecToStringData.insert(AV_CODEC_ID_MUSEPACK8, "MUSEPACK8");
    codecToStringData.insert(AV_CODEC_ID_SPEEX, "SPEEX");
    codecToStringData.insert(AV_CODEC_ID_WMAVOICE, "WMA VOICE");
    codecToStringData.insert(AV_CODEC_ID_WMAPRO, "WMA PRO");
    codecToStringData.insert(AV_CODEC_ID_WMALOSSLESS, "WMA LOSSLESS");
    codecToStringData.insert(AV_CODEC_ID_ATRAC3P, "ATRAC3P");
    codecToStringData.insert(AV_CODEC_ID_EAC3, "EAC3");
    codecToStringData.insert(AV_CODEC_ID_SIPR, "SIPR");
    codecToStringData.insert(AV_CODEC_ID_MP1, "MP1");
    codecToStringData.insert(AV_CODEC_ID_TWINVQ, "TWINVQ");
    codecToStringData.insert(AV_CODEC_ID_TRUEHD, "TRUE-HD");
    codecToStringData.insert(AV_CODEC_ID_MP4ALS, "MP4ALS");
    codecToStringData.insert(AV_CODEC_ID_ATRAC1, "ATRAC1");
    codecToStringData.insert(AV_CODEC_ID_BINKAUDIO_RDFT, "BINKAUDIO RDFT");
    codecToStringData.insert(AV_CODEC_ID_BINKAUDIO_DCT, "BINKAUDIO DCT");
    codecToStringData.insert(AV_CODEC_ID_AAC_LATM, "AAC LATM");
    codecToStringData.insert(AV_CODEC_ID_QDMC, "QDMC");
    codecToStringData.insert(AV_CODEC_ID_CELT, "CELT");

    /* subtitle codecs */
    codecToStringData.insert(AV_CODEC_ID_DVD_SUBTITLE, "DVD_SUBTITLE");
    codecToStringData.insert(AV_CODEC_ID_DVB_SUBTITLE, "DVB_SUBTITLE");
    codecToStringData.insert(AV_CODEC_ID_TEXT, "TEXT"); //< Raw UTF-8 text.
    codecToStringData.insert(AV_CODEC_ID_XSUB, "XSUB");
    codecToStringData.insert(AV_CODEC_ID_SSA, "SSA");
    codecToStringData.insert(AV_CODEC_ID_MOV_TEXT, "MOV_TEXT");
    codecToStringData.insert(AV_CODEC_ID_HDMV_PGS_SUBTITLE, "HDMV_PGS_SUBTITLE");
    codecToStringData.insert(AV_CODEC_ID_DVB_TELETEXT, "DVB_TELETEXT");
    codecToStringData.insert(AV_CODEC_ID_SRT, "SRT");
    codecToStringData.insert(AV_CODEC_ID_MICRODVD, "MICRODVD");

    /* other specific kind of codecs (generally used for attachments) */
    codecToStringData.insert(AV_CODEC_ID_TTF, "TTF");

    // codec_id is not known (like AV_CODEC_ID_NONE) but lavf should attempt to
    // identify it.
    codecToStringData.insert(AV_CODEC_ID_PROBE, "PROBE");

    // _FAKE_ codec to indicate a raw MPEG-2 TS stream (only used by
    // libavformat).
    codecToStringData.insert(AV_CODEC_ID_MPEG2TS, "MPEG2TS");

    // Dummy codec for streams containing only metadata information.
    codecToStringData.insert(AV_CODEC_ID_FFMETADATA, "FFMETADATA");

    codecToStringData.insert(AV_CODEC_ID_NONE, "");

    return codecToStringData;
}

QString QnAvCodecHelper::codecIdToString(AVCodecID codecId)
{
    return QString::fromLatin1(codecToStringData().value(codecId));
}

AVCodecID QnAvCodecHelper::codecIdFromString(const QString& value)
{
    static QnMutex mutex;
    QnMutexLocker lock(&mutex);

    QMap<QByteArray, AVCodecID> stringToCodecData;
    if (!stringToCodecData.isEmpty())
        return stringToCodecData.value(value.toLatin1());
    const auto codecData = codecToStringData();
    for (auto itr = codecData.begin(); itr != codecData.end(); ++itr)
        stringToCodecData.insert(itr.value(), itr.key());
    return stringToCodecData.value(value.toLatin1());
}
