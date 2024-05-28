// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "av_codec_helper.h"

#include <array>

#include <QtCore/QByteArray>

#include <nx/utils/log/assert.h>
#include <nx/utils/thread/mutex.h>

static const QMap<AVCodecID, QByteArray> kCodecToStringData =
{
    {AV_CODEC_ID_MPEG1VIDEO, "MPEG1VIDEO"},
    {AV_CODEC_ID_MPEG2VIDEO, "MPEG2VIDEO"}, //< Preferred id for MPEG-1/2 video decoding.
    {AV_CODEC_ID_H261, "H261"},
    {AV_CODEC_ID_H263, "H263"},
    {AV_CODEC_ID_RV10, "RV10"},
    {AV_CODEC_ID_RV20, "RV20"},
    {AV_CODEC_ID_MJPEG, "MJPEG"},
    {AV_CODEC_ID_MJPEGB, "MJPEGB"},
    {AV_CODEC_ID_LJPEG, "LJPEG"},
    {AV_CODEC_ID_SP5X, "SP5X"},
    {AV_CODEC_ID_JPEGLS, "JPEGLS"},
    {AV_CODEC_ID_MPEG4, "MPEG4"},
    {AV_CODEC_ID_RAWVIDEO, "RAWVIDEO"},
    {AV_CODEC_ID_MSMPEG4V1, "MSMPEG4V1"},
    {AV_CODEC_ID_MSMPEG4V2, "MSMPEG4V2"},
    {AV_CODEC_ID_MSMPEG4V3, "MSMPEG4V3"},
    {AV_CODEC_ID_WMV1, "WMV1"},
    {AV_CODEC_ID_WMV2, "WMV2"},
    {AV_CODEC_ID_H263P, "H263P"},
    {AV_CODEC_ID_H263I, "H263I"},
    {AV_CODEC_ID_FLV1, "FLV1"},
    {AV_CODEC_ID_SVQ1, "SVQ1"},
    {AV_CODEC_ID_SVQ3, "SVQ3"},
    {AV_CODEC_ID_DVVIDEO, "DVVIDEO"},
    {AV_CODEC_ID_HUFFYUV, "HUFFYUV"},
    {AV_CODEC_ID_CYUV, "CYUV"},
    {AV_CODEC_ID_H264, "H264"},
    {AV_CODEC_ID_HEVC, "H265"},
    {AV_CODEC_ID_INDEO3, "INDEO3"},
    {AV_CODEC_ID_VP3, "VP3"},
    {AV_CODEC_ID_THEORA, "THEORA"},
    {AV_CODEC_ID_ASV1, "ASV1"},
    {AV_CODEC_ID_ASV2, "ASV2"},
    {AV_CODEC_ID_FFV1, "FFV1"},
    {AV_CODEC_ID_4XM, "4XM"},
    {AV_CODEC_ID_VCR1, "VCR1"},
    {AV_CODEC_ID_CLJR, "CLJR"},
    {AV_CODEC_ID_MDEC, "MDEC"},
    {AV_CODEC_ID_ROQ, "ROQ"},
    {AV_CODEC_ID_INTERPLAY_VIDEO, "INTERPLAY_VIDEO"},
    {AV_CODEC_ID_XAN_WC3, "XAN_WC3"},
    {AV_CODEC_ID_XAN_WC4, "XAN_WC4"},
    {AV_CODEC_ID_RPZA, "RPZA"},
    {AV_CODEC_ID_CINEPAK, "CINEPAK"},
    {AV_CODEC_ID_WS_VQA, "WS_VQA"},
    {AV_CODEC_ID_MSRLE, "MSRLE"},
    {AV_CODEC_ID_MSVIDEO1, "MSVIDEO1"},
    {AV_CODEC_ID_IDCIN, "IDCIN"},
    {AV_CODEC_ID_8BPS, "8BPS"},
    {AV_CODEC_ID_SMC, "SMC"},
    {AV_CODEC_ID_FLIC, "FLIC"},
    {AV_CODEC_ID_TRUEMOTION1, "TRUEMOTION1"},
    {AV_CODEC_ID_VMDVIDEO, "VMDVIDEO"},
    {AV_CODEC_ID_MSZH, "MSZH"},
    {AV_CODEC_ID_ZLIB, "ZLIB"},
    {AV_CODEC_ID_QTRLE, "QTRLE"},
    {AV_CODEC_ID_SNOW, "SNOW"},
    {AV_CODEC_ID_TSCC, "TSCC"},
    {AV_CODEC_ID_ULTI, "ULTI"},
    {AV_CODEC_ID_QDRAW, "QDRAW"},
    {AV_CODEC_ID_VIXL, "VIXL"},
    {AV_CODEC_ID_QPEG, "QPEG"},
    {AV_CODEC_ID_PNG, "PNG"},
    {AV_CODEC_ID_PPM, "PPM"},
    {AV_CODEC_ID_PBM, "PBM"},
    {AV_CODEC_ID_PGM, "PGM"},
    {AV_CODEC_ID_PGMYUV, "PGMYUV"},
    {AV_CODEC_ID_PAM, "PAM"},
    {AV_CODEC_ID_FFVHUFF, "FFVHUFF"},
    {AV_CODEC_ID_RV30, "RV30"},
    {AV_CODEC_ID_RV40, "RV40"},
    {AV_CODEC_ID_VC1, "VC1"},
    {AV_CODEC_ID_WMV3, "WMV3"},
    {AV_CODEC_ID_LOCO, "LOCO"},
    {AV_CODEC_ID_WNV1, "WNV1"},
    {AV_CODEC_ID_AASC, "AASC"},
    {AV_CODEC_ID_INDEO2, "INDEO2"},
    {AV_CODEC_ID_FRAPS, "FRAPS"},
    {AV_CODEC_ID_TRUEMOTION2, "TRUEMOTION2"},
    {AV_CODEC_ID_BMP, "BMP"},
    {AV_CODEC_ID_CSCD, "CSCD"},
    {AV_CODEC_ID_MMVIDEO, "MMVIDEO"},
    {AV_CODEC_ID_ZMBV, "ZMBV"},
    {AV_CODEC_ID_AVS, "AVS"},
    {AV_CODEC_ID_SMACKVIDEO, "SMACKVIDEO"},
    {AV_CODEC_ID_NUV, "NUV"},
    {AV_CODEC_ID_KMVC, "KMVC"},
    {AV_CODEC_ID_FLASHSV, "FLASHSV"},
    {AV_CODEC_ID_CAVS, "CAVS"},
    {AV_CODEC_ID_JPEG2000, "JPEG2000"},
    {AV_CODEC_ID_VMNC, "VMNC"},
    {AV_CODEC_ID_VP5, "VP5"},
    {AV_CODEC_ID_VP6, "VP6"},
    {AV_CODEC_ID_VP6F, "VP6F"},
    {AV_CODEC_ID_TARGA, "TARGA"},
    {AV_CODEC_ID_DSICINVIDEO, "DSICINVIDEO"},
    {AV_CODEC_ID_TIERTEXSEQVIDEO, "TIERTEXSEQVIDEO"},
    {AV_CODEC_ID_TIFF, "TIFF"},
    {AV_CODEC_ID_GIF, "GIF"},
    //{AV_CODEC_ID_FFH264,  "FFH264"},
    {AV_CODEC_ID_DXA, "DXA"},
    {AV_CODEC_ID_DNXHD, "DNXHD"},
    {AV_CODEC_ID_THP, "THP"},
    {AV_CODEC_ID_SGI, "SGI"},
    {AV_CODEC_ID_C93, "C93"},
    {AV_CODEC_ID_BETHSOFTVID, "BETHSOFTVID"},
    {AV_CODEC_ID_PTX, "PTX"},
    {AV_CODEC_ID_TXD, "TXD"},
    {AV_CODEC_ID_VP6A, "VP6A"},
    {AV_CODEC_ID_AMV, "AMV"},
    {AV_CODEC_ID_VB, "VB"},
    {AV_CODEC_ID_PCX, "PCX"},
    {AV_CODEC_ID_SUNRAST, "SUNRAST"},
    {AV_CODEC_ID_INDEO4, "INDEO4"},
    {AV_CODEC_ID_INDEO5, "INDEO5"},
    {AV_CODEC_ID_MIMIC, "MIMIC"},
    {AV_CODEC_ID_RL2, "RL2"},
    {AV_CODEC_ID_8SVX_EXP, "8SVX_EXP"},
    {AV_CODEC_ID_8SVX_FIB, "8SVX_FIB"},
    {AV_CODEC_ID_ESCAPE124, "ESCAPE124"},
    {AV_CODEC_ID_DIRAC, "DIRAC"},
    {AV_CODEC_ID_BFI, "BFI"},
    {AV_CODEC_ID_CMV, "CMV"},
    {AV_CODEC_ID_MOTIONPIXELS, "MOTIONPIXELS"},
    {AV_CODEC_ID_TGV, "TGV"},
    {AV_CODEC_ID_TGQ, "TGQ"},
    {AV_CODEC_ID_TQI, "TQI"},
    {AV_CODEC_ID_AURA, "AURA"},
    {AV_CODEC_ID_AURA2, "AURA2"},
    {AV_CODEC_ID_V210X, "V210X"},
    {AV_CODEC_ID_TMV, "TMV"},
    {AV_CODEC_ID_V210, "V210"},
    {AV_CODEC_ID_DPX, "DPX"},
    {AV_CODEC_ID_MAD, "MAD"},
    {AV_CODEC_ID_FRWU, "FRWU"},
    {AV_CODEC_ID_FLASHSV2, "FLASHSV2"},
    {AV_CODEC_ID_CDGRAPHICS, "CDGRAPHICS"},
    {AV_CODEC_ID_R210, "R210"},
    {AV_CODEC_ID_ANM, "ANM"},
    {AV_CODEC_ID_BINKVIDEO, "BINKVIDEO"},
    {AV_CODEC_ID_IFF_ILBM, "IFF_ILBM"},
    // {AV_CODEC_ID_IFF_BYTERUN1,  "BYTERUN1"},
    {AV_CODEC_ID_KGV1, "KGV1"},
    {AV_CODEC_ID_YOP, "YOP"},
    {AV_CODEC_ID_VP8, "VP8"},
    {AV_CODEC_ID_PICTOR, "PICTOR"},
    {AV_CODEC_ID_ANSI, "ANSI"},
    {AV_CODEC_ID_A64_MULTI, "A64_MULTI"},
    {AV_CODEC_ID_A64_MULTI5, "A64_MULTI5"},
    {AV_CODEC_ID_R10K, "R10K"},
    {AV_CODEC_ID_MXPEG, "MXPEG"},
    {AV_CODEC_ID_LAGARITH, "LAGARITH"},
    {AV_CODEC_ID_PRORES, "PRORES"},
    {AV_CODEC_ID_JV, "JV"},
    {AV_CODEC_ID_DFA, "DFA"},
    // {AV_CODEC_ID_8SVX_RAW,  "8SVX_RAW"},

    /* various PCM "codecs" */
    {AV_CODEC_ID_PCM_S16LE, "PCM"},
    {AV_CODEC_ID_PCM_S16BE, "PCM"},
    {AV_CODEC_ID_PCM_U16LE, "PCM"},
    {AV_CODEC_ID_PCM_U16BE, "PCM"},
    {AV_CODEC_ID_PCM_S8, "PCM"},
    {AV_CODEC_ID_PCM_U8, "PCM"},
    {AV_CODEC_ID_PCM_MULAW, "PCM"},
    {AV_CODEC_ID_PCM_ALAW, "PCM"},
    {AV_CODEC_ID_PCM_S32LE, "PCM"},
    {AV_CODEC_ID_PCM_S32BE, "PCM"},
    {AV_CODEC_ID_PCM_U32LE, "PCM"},
    {AV_CODEC_ID_PCM_U32BE, "PCM"},
    {AV_CODEC_ID_PCM_S24LE, "PCM"},
    {AV_CODEC_ID_PCM_S24BE, "PCM"},
    {AV_CODEC_ID_PCM_U24LE, "PCM"},
    {AV_CODEC_ID_PCM_U24BE, "PCM"},
    {AV_CODEC_ID_PCM_S24DAUD, "PCM"},
    {AV_CODEC_ID_PCM_ZORK, "PCM"},
    {AV_CODEC_ID_PCM_S16LE_PLANAR, "PCM"},
    {AV_CODEC_ID_PCM_DVD, "PCM"},
    {AV_CODEC_ID_PCM_F32BE, "PCM"},
    {AV_CODEC_ID_PCM_F32LE, "PCM"},
    {AV_CODEC_ID_PCM_F64BE, "PCM"},
    {AV_CODEC_ID_PCM_F64LE, "PCM"},
    {AV_CODEC_ID_PCM_BLURAY, "PCM"},
    {AV_CODEC_ID_PCM_LXF, "PCM"},
    {AV_CODEC_ID_S302M, "PCM"},

    /* various ADPCM codecs */
    {AV_CODEC_ID_ADPCM_IMA_QT, "PCM"},
    {AV_CODEC_ID_ADPCM_IMA_WAV, "PCM"},
    {AV_CODEC_ID_ADPCM_IMA_DK3, "PCM"},
    {AV_CODEC_ID_ADPCM_IMA_DK4, "PCM"},
    {AV_CODEC_ID_ADPCM_IMA_WS, "PCM"},
    {AV_CODEC_ID_ADPCM_IMA_SMJPEG, "PCM"},
    {AV_CODEC_ID_ADPCM_MS, "PCM"},
    {AV_CODEC_ID_ADPCM_4XM, "PCM"},
    {AV_CODEC_ID_ADPCM_XA, "PCM"},
    {AV_CODEC_ID_ADPCM_ADX, "PCM"},
    {AV_CODEC_ID_ADPCM_EA, "PCM"},
    {AV_CODEC_ID_ADPCM_G726, "PCM"},
    {AV_CODEC_ID_ADPCM_CT, "PCM"},
    {AV_CODEC_ID_ADPCM_SWF, "PCM"},
    {AV_CODEC_ID_ADPCM_YAMAHA, "PCM"},
    {AV_CODEC_ID_ADPCM_SBPRO_4, "PCM"},
    {AV_CODEC_ID_ADPCM_SBPRO_3, "PCM"},
    {AV_CODEC_ID_ADPCM_SBPRO_2, "PCM"},
    {AV_CODEC_ID_ADPCM_THP, "PCM"},
    {AV_CODEC_ID_ADPCM_IMA_AMV, "PCM"},
    {AV_CODEC_ID_ADPCM_EA_R1, "PCM"},
    {AV_CODEC_ID_ADPCM_EA_R3, "PCM"},
    {AV_CODEC_ID_ADPCM_EA_R2, "PCM"},
    {AV_CODEC_ID_ADPCM_IMA_EA_SEAD, "PCM"},
    {AV_CODEC_ID_ADPCM_IMA_EA_EACS, "PCM"},
    {AV_CODEC_ID_ADPCM_EA_XAS, "PCM"},
    {AV_CODEC_ID_ADPCM_EA_MAXIS_XA, "PCM"},
    {AV_CODEC_ID_ADPCM_IMA_ISS, "PCM"},
    {AV_CODEC_ID_ADPCM_G722, "PCM"},

    /* AMR */
    {AV_CODEC_ID_AMR_NB, "AMR-NB"},
    {AV_CODEC_ID_AMR_WB, "AMR-WB"},

    /* RealAudio codecs*/
    {AV_CODEC_ID_RA_144, "RA-144"},
    {AV_CODEC_ID_RA_288, "RA-288"},

    /* various DPCM codecs */
    {AV_CODEC_ID_ROQ_DPCM, "PCM"},
    {AV_CODEC_ID_INTERPLAY_DPCM, "PCM"},
    {AV_CODEC_ID_XAN_DPCM, "PCM"},
    {AV_CODEC_ID_SOL_DPCM, "PCM"},

    /* audio codecs */
    {AV_CODEC_ID_MP2, "MP2"},
    {AV_CODEC_ID_MP3, "MP3"}, //< Preferred id for decoding MPEG audio layer 1,  ""}, 2 or 3.
    {AV_CODEC_ID_AAC, "AAC"},
    {AV_CODEC_ID_AC3, "Dolby"},
    {AV_CODEC_ID_DTS, "DTS"},
    {AV_CODEC_ID_VORBIS, "VORBIS"},
    {AV_CODEC_ID_DVAUDIO, "DVAUDIO"},
    {AV_CODEC_ID_WMAV1, "WMAV1"},
    {AV_CODEC_ID_WMAV2, "WMAV2"},
    {AV_CODEC_ID_MACE3, "MACE3"},
    {AV_CODEC_ID_MACE6, "MACE6"},
    {AV_CODEC_ID_VMDAUDIO, "VMDAUDIO"},
    {AV_CODEC_ID_SONIC, "SONIC"},
    {AV_CODEC_ID_SONIC_LS, "SONIC_LC"},
    {AV_CODEC_ID_FLAC, "FLAC"},
    {AV_CODEC_ID_MP3ADU, "MP3"},
    {AV_CODEC_ID_MP3ON4, "MP3"},
    {AV_CODEC_ID_SHORTEN, "SHORTEN"},
    {AV_CODEC_ID_ALAC, "ALAC"},
    {AV_CODEC_ID_WESTWOOD_SND1, "WESTWOOD"},
    {AV_CODEC_ID_GSM, "GSM"}, //< As in Berlin toast format.
    {AV_CODEC_ID_QDM2, "QDM2"},
    {AV_CODEC_ID_COOK, "COOK"},
    {AV_CODEC_ID_TRUESPEECH, "TRUESPEECH"},
    {AV_CODEC_ID_TTA, "TTA"},
    {AV_CODEC_ID_SMACKAUDIO, "SMACKAUDIO"},
    {AV_CODEC_ID_QCELP, "QCELP"},
    {AV_CODEC_ID_WAVPACK, "WAVPACK"},
    {AV_CODEC_ID_DSICINAUDIO, "DSICINAUDIO"},
    {AV_CODEC_ID_IMC, "IMC"},
    {AV_CODEC_ID_MUSEPACK7, "MUSEPACK7"},
    {AV_CODEC_ID_MLP, "MLP"},
    {AV_CODEC_ID_GSM_MS, "GSM_MS"}, //< As found in WAV.
    {AV_CODEC_ID_ATRAC3, "ATRAC3"},
    {AV_CODEC_ID_APE, "APE"},
    {AV_CODEC_ID_NELLYMOSER, "NELLYMOSER"},
    {AV_CODEC_ID_MUSEPACK8, "MUSEPACK8"},
    {AV_CODEC_ID_SPEEX, "SPEEX"},
    {AV_CODEC_ID_WMAVOICE, "WMA VOICE"},
    {AV_CODEC_ID_WMAPRO, "WMA PRO"},
    {AV_CODEC_ID_WMALOSSLESS, "WMA LOSSLESS"},
    {AV_CODEC_ID_ATRAC3P, "ATRAC3P"},
    {AV_CODEC_ID_EAC3, "EAC3"},
    {AV_CODEC_ID_SIPR, "SIPR"},
    {AV_CODEC_ID_MP1, "MP1"},
    {AV_CODEC_ID_TWINVQ, "TWINVQ"},
    {AV_CODEC_ID_TRUEHD, "TRUE-HD"},
    {AV_CODEC_ID_MP4ALS, "MP4ALS"},
    {AV_CODEC_ID_ATRAC1, "ATRAC1"},
    {AV_CODEC_ID_BINKAUDIO_RDFT, "BINKAUDIO RDFT"},
    {AV_CODEC_ID_BINKAUDIO_DCT, "BINKAUDIO DCT"},
    {AV_CODEC_ID_AAC_LATM, "AAC LATM"},
    {AV_CODEC_ID_QDMC, "QDMC"},
    {AV_CODEC_ID_CELT, "CELT"},

    /* subtitle codecs */
    {AV_CODEC_ID_DVD_SUBTITLE, "DVD_SUBTITLE"},
    {AV_CODEC_ID_DVB_SUBTITLE, "DVB_SUBTITLE"},
    {AV_CODEC_ID_TEXT, "TEXT"}, //< Raw UTF-8 text.
    {AV_CODEC_ID_XSUB, "XSUB"},
    {AV_CODEC_ID_SSA, "SSA"},
    {AV_CODEC_ID_MOV_TEXT, "MOV_TEXT"},
    {AV_CODEC_ID_HDMV_PGS_SUBTITLE, "HDMV_PGS_SUBTITLE"},
    {AV_CODEC_ID_DVB_TELETEXT, "DVB_TELETEXT"},
    {AV_CODEC_ID_SRT, "SRT"},
    {AV_CODEC_ID_MICRODVD, "MICRODVD"},

    /* other specific kind of codecs (generally used for attachments) */
    {AV_CODEC_ID_TTF, "TTF"},

    // codec_id is not known (like AV_CODEC_ID_NONE) but lavf should attempt to
    // identify it.
    {AV_CODEC_ID_PROBE, "PROBE"},

    // _FAKE_ codec to indicate a raw MPEG-2 TS stream (only used by
    // libavformat).
    {AV_CODEC_ID_MPEG2TS, "MPEG2TS"},

    // Dummy codec for streams containing only metadata information.
    {AV_CODEC_ID_FFMETADATA, "FFMETADATA"},

    {AV_CODEC_ID_NONE, ""},

    { AV_CODEC_ID_MPEG1VIDEO, "MPEG1VIDEO" }
};

static const QMap<QByteArray, AVCodecID> kStringToCodecData =
    []()
    {
        QMap<QByteArray, AVCodecID> result;
        for (auto itr = kCodecToStringData.begin(); itr != kCodecToStringData.end(); ++itr)
            result.insert(itr.value(), itr.key());
        return result;
    }();

QString QnAvCodecHelper::codecIdToString(AVCodecID codecId)
{
    return QString::fromLatin1(kCodecToStringData.value(codecId));
}

AVCodecID QnAvCodecHelper::codecIdFromString(const QString& value)
{
    return kStringToCodecData.value(value.toLatin1(), AV_CODEC_ID_NONE);
}
