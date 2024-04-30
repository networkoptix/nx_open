/*
* Copyright 2017-2021 NVIDIA Corporation.  All rights reserved.
*
* Please refer to the NVIDIA end user license agreement (EULA) associated
* with this source code for terms and conditions that govern your use of
* this software. Any use, reproduction, disclosure, or distribution of
* this software and related documentation outside the terms of the EULA
* is strictly prohibited.
*
*/
#include <iostream>
#include <algorithm>
#include <chrono>
#include <cmath>

#include "NvDecoder.h"
#include <nx/utils/log/log.h>

#include <nx/media/nvidia/nvidia_driver_proxy.h>

using namespace nx::media::nvidia;

#define START_TIMER auto start = std::chrono::high_resolution_clock::now();

#define STOP_TIMER(print_message) int64_t elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>( \
    std::chrono::high_resolution_clock::now() - start).count(); \
    std::cout << print_message << \
    elapsedTime \
    << " ms " << std::endl;

#define CUDA_DRVAPI_CALL( call )                                                                                                 \
    do                                                                                                                           \
    {                                                                                                                            \
        CUresult err__ = call;                                                                                                   \
        if (err__ != CUDA_SUCCESS)                                                                                               \
        {                                                                                                                        \
            const char *szErrName = NULL;                                                                                        \
            NvidiaDriverApiProxy::instance().cuGetErrorName(err__, &szErrName);                                                                                   \
            std::ostringstream errorLog;                                                                                         \
            errorLog << "CUDA driver API error " << szErrName ;                                                                  \
            throw NVDECException::makeNVDECException(errorLog.str(), err__, __FUNCTION__, __FILE__, __LINE__);                   \
        }                                                                                                                        \
    }                                                                                                                            \
    while (0)

static const char * GetVideoCodecString(cudaVideoCodec eCodec) {
    static struct {
        cudaVideoCodec eCodec;
        const char *name;
    } aCodecName [] = {
        { cudaVideoCodec_MPEG1,     "MPEG-1"       },
        { cudaVideoCodec_MPEG2,     "MPEG-2"       },
        { cudaVideoCodec_MPEG4,     "MPEG-4 (ASP)" },
        { cudaVideoCodec_VC1,       "VC-1/WMV"     },
        { cudaVideoCodec_H264,      "AVC/H.264"    },
        { cudaVideoCodec_JPEG,      "M-JPEG"       },
        { cudaVideoCodec_H264_SVC,  "H.264/SVC"    },
        { cudaVideoCodec_H264_MVC,  "H.264/MVC"    },
        { cudaVideoCodec_HEVC,      "H.265/HEVC"   },
        { cudaVideoCodec_VP8,       "VP8"          },
        { cudaVideoCodec_VP9,       "VP9"          },
        { cudaVideoCodec_AV1,       "AV1"          },
        { cudaVideoCodec_NumCodecs, "Invalid"      },
        { cudaVideoCodec_YUV420,    "YUV  4:2:0"   },
        { cudaVideoCodec_YV12,      "YV12 4:2:0"   },
        { cudaVideoCodec_NV12,      "NV12 4:2:0"   },
        { cudaVideoCodec_YUYV,      "YUYV 4:2:2"   },
        { cudaVideoCodec_UYVY,      "UYVY 4:2:2"   },
    };

    if (eCodec >= 0 && eCodec <= cudaVideoCodec_NumCodecs) {
        return aCodecName[eCodec].name;
    }
    for (int i = cudaVideoCodec_NumCodecs + 1; i < (int)sizeof(aCodecName) / sizeof(aCodecName[0]); i++) {
        if (eCodec == aCodecName[i].eCodec) {
            return aCodecName[eCodec].name;
        }
    }
    return "Unknown";
}

static const char * GetVideoChromaFormatString(cudaVideoChromaFormat eChromaFormat) {
    static struct {
        cudaVideoChromaFormat eChromaFormat;
        const char *name;
    } aChromaFormatName[] = {
        { cudaVideoChromaFormat_Monochrome, "YUV 400 (Monochrome)" },
        { cudaVideoChromaFormat_420,        "YUV 420"              },
        { cudaVideoChromaFormat_422,        "YUV 422"              },
        { cudaVideoChromaFormat_444,        "YUV 444"              },
    };

    if (eChromaFormat >= 0 && eChromaFormat < (int)sizeof(aChromaFormatName) / sizeof(aChromaFormatName[0])) {
        return aChromaFormatName[eChromaFormat].name;
    }
    return "Unknown";
}

static float GetChromaHeightFactor(cudaVideoSurfaceFormat eSurfaceFormat)
{
    float factor = 0.5;
    switch (eSurfaceFormat)
    {
    case cudaVideoSurfaceFormat_NV12:
    case cudaVideoSurfaceFormat_P016:
        factor = 0.5;
        break;
    case cudaVideoSurfaceFormat_YUV444:
    case cudaVideoSurfaceFormat_YUV444_16Bit:
        factor = 1.0;
        break;
    }

    return factor;
}

static int GetChromaPlaneCount(cudaVideoSurfaceFormat eSurfaceFormat)
{
    int numPlane = 1;
    switch (eSurfaceFormat)
    {
    case cudaVideoSurfaceFormat_NV12:
    case cudaVideoSurfaceFormat_P016:
        numPlane = 1;
        break;
    case cudaVideoSurfaceFormat_YUV444:
    case cudaVideoSurfaceFormat_YUV444_16Bit:
        numPlane = 2;
        break;
    }

    return numPlane;
}

std::map<int, int64_t> NvDecoder::sessionOverHead = { {0,0}, {1,0} };

/* Called when the parser encounters sequence header for AV1 SVC content
*  return value interpretation:
*      < 0 : fail, >=0: succeeded (bit 0-9: currOperatingPoint, bit 10-10: bDispAllLayer, bit 11-30: reserved, must be set 0)
*/
int NvDecoder::GetOperatingPoint(CUVIDOPERATINGPOINTINFO *pOPInfo)
{
    if (pOPInfo->codec == cudaVideoCodec_AV1)
    {
        if (pOPInfo->av1.operating_points_cnt > 1)
        {
            // clip has SVC enabled
            if (m_nOperatingPoint >= pOPInfo->av1.operating_points_cnt)
                m_nOperatingPoint = 0;

            printf("AV1 SVC clip: operating point count %d  ", pOPInfo->av1.operating_points_cnt);
            printf("Selected operating point: %d, IDC 0x%x bOutputAllLayers %d\n", m_nOperatingPoint, pOPInfo->av1.operating_points_idc[m_nOperatingPoint], m_bDispAllLayers);
            return (m_nOperatingPoint | (m_bDispAllLayers << 10));
        }
    }
    return -1;
}

/* Return value from HandleVideoSequence() are interpreted as   :
*  0: fail, 1: succeeded, > 1: override dpb size of parser (set by CUVIDPARSERPARAMS::ulMaxNumDecodeSurfaces while creating parser)
*/
int NvDecoder::HandleVideoSequence(CUVIDEOFORMAT *pVideoFormat)
{
    START_TIMER
    m_videoInfo.str("");
    m_videoInfo.clear();
    m_videoInfo << "Video Input Information" << std::endl
        << "\tCodec        : " << GetVideoCodecString(pVideoFormat->codec) << std::endl
        << "\tFrame rate   : " << pVideoFormat->frame_rate.numerator << "/" << pVideoFormat->frame_rate.denominator
            << " = " << 1.0 * pVideoFormat->frame_rate.numerator / pVideoFormat->frame_rate.denominator << " fps" << std::endl
        << "\tSequence     : " << (pVideoFormat->progressive_sequence ? "Progressive" : "Interlaced") << std::endl
        << "\tCoded size   : [" << pVideoFormat->coded_width << ", " << pVideoFormat->coded_height << "]" << std::endl
        << "\tDisplay area : [" << pVideoFormat->display_area.left << ", " << pVideoFormat->display_area.top << ", "
            << pVideoFormat->display_area.right << ", " << pVideoFormat->display_area.bottom << "]" << std::endl
        << "\tChroma       : " << GetVideoChromaFormatString(pVideoFormat->chroma_format) << std::endl
        << "\tBit depth    : " << pVideoFormat->bit_depth_luma_minus8 + 8
    ;
    m_videoInfo << std::endl;

    int nDecodeSurface = pVideoFormat->min_num_decode_surfaces;

    CUVIDDECODECAPS decodecaps;
    memset(&decodecaps, 0, sizeof(decodecaps));

    decodecaps.eCodecType = pVideoFormat->codec;
    decodecaps.eChromaFormat = pVideoFormat->chroma_format;
    decodecaps.nBitDepthMinus8 = pVideoFormat->bit_depth_luma_minus8;

    CUDA_DRVAPI_CALL(NvidiaDriverApiProxy::instance().cuCtxPushCurrent(m_cuContext));
    NVDEC_API_CALL(NvidiaDriverDecoderProxy::instance().cuvidGetDecoderCaps(&decodecaps));
    CUDA_DRVAPI_CALL(NvidiaDriverApiProxy::instance().cuCtxPopCurrent(NULL));

    if(!decodecaps.bIsSupported){
        NVDEC_THROW_ERROR("Codec not supported on this GPU", CUDA_ERROR_NOT_SUPPORTED);
        return nDecodeSurface;
    }

    if ((pVideoFormat->coded_width > decodecaps.nMaxWidth) ||
        (pVideoFormat->coded_height > decodecaps.nMaxHeight)){

        std::ostringstream errorString;
        errorString << std::endl
                    << "Resolution          : " << pVideoFormat->coded_width << "x" << pVideoFormat->coded_height << std::endl
                    << "Max Supported (wxh) : " << decodecaps.nMaxWidth << "x" << decodecaps.nMaxHeight << std::endl
                    << "Resolution not supported on this GPU";

        const std::string cErr = errorString.str();
        NVDEC_THROW_ERROR(cErr, CUDA_ERROR_NOT_SUPPORTED);
        return nDecodeSurface;
    }

    if ((pVideoFormat->coded_width>>4)*(pVideoFormat->coded_height>>4) > decodecaps.nMaxMBCount){

        std::ostringstream errorString;
        errorString << std::endl
                    << "MBCount             : " << (pVideoFormat->coded_width >> 4)*(pVideoFormat->coded_height >> 4) << std::endl
                    << "Max Supported mbcnt : " << decodecaps.nMaxMBCount << std::endl
                    << "MBCount not supported on this GPU";

        const std::string cErr = errorString.str();
        NVDEC_THROW_ERROR(cErr, CUDA_ERROR_NOT_SUPPORTED);
        return nDecodeSurface;
    }

    if (m_nWidth && m_nLumaHeight && m_nChromaHeight) {

        // cuvidCreateDecoder() has been called before, and now there's possible config change
        return ReconfigureDecoder(pVideoFormat);
    }

    // eCodec has been set in the constructor (for parser). Here it's set again for potential correction
    m_eCodec = pVideoFormat->codec;
    m_eChromaFormat = pVideoFormat->chroma_format;
    m_nBitDepthMinus8 = pVideoFormat->bit_depth_luma_minus8;
    m_nBPP = m_nBitDepthMinus8 > 0 ? 2 : 1;

    // Set the output surface format same as chroma format
    if (m_eChromaFormat == cudaVideoChromaFormat_420 || cudaVideoChromaFormat_Monochrome)
        m_eOutputFormat = pVideoFormat->bit_depth_luma_minus8 ? cudaVideoSurfaceFormat_P016 : cudaVideoSurfaceFormat_NV12;
    else if (m_eChromaFormat == cudaVideoChromaFormat_444)
        m_eOutputFormat = pVideoFormat->bit_depth_luma_minus8 ? cudaVideoSurfaceFormat_YUV444_16Bit : cudaVideoSurfaceFormat_YUV444;
    else if (m_eChromaFormat == cudaVideoChromaFormat_422)
        m_eOutputFormat = cudaVideoSurfaceFormat_NV12;  // no 4:2:2 output format supported yet so make 420 default

    // Check if output format supported. If not, check falback options
    if (!(decodecaps.nOutputFormatMask & (1 << m_eOutputFormat)))
    {
        if (decodecaps.nOutputFormatMask & (1 << cudaVideoSurfaceFormat_NV12))
            m_eOutputFormat = cudaVideoSurfaceFormat_NV12;
        else if (decodecaps.nOutputFormatMask & (1 << cudaVideoSurfaceFormat_P016))
            m_eOutputFormat = cudaVideoSurfaceFormat_P016;
        else if (decodecaps.nOutputFormatMask & (1 << cudaVideoSurfaceFormat_YUV444))
            m_eOutputFormat = cudaVideoSurfaceFormat_YUV444;
        else if (decodecaps.nOutputFormatMask & (1 << cudaVideoSurfaceFormat_YUV444_16Bit))
            m_eOutputFormat = cudaVideoSurfaceFormat_YUV444_16Bit;
        else
            NVDEC_THROW_ERROR("No supported output format found", CUDA_ERROR_NOT_SUPPORTED);
    }
    m_videoFormat = *pVideoFormat;

    CUVIDDECODECREATEINFO videoDecodeCreateInfo = { 0 };
    videoDecodeCreateInfo.CodecType = pVideoFormat->codec;
    videoDecodeCreateInfo.ChromaFormat = pVideoFormat->chroma_format;
    videoDecodeCreateInfo.OutputFormat = m_eOutputFormat;
    videoDecodeCreateInfo.bitDepthMinus8 = pVideoFormat->bit_depth_luma_minus8;
    if (pVideoFormat->progressive_sequence)
        videoDecodeCreateInfo.DeinterlaceMode = cudaVideoDeinterlaceMode_Weave;
    else
        videoDecodeCreateInfo.DeinterlaceMode = cudaVideoDeinterlaceMode_Adaptive;
    videoDecodeCreateInfo.ulNumOutputSurfaces = 2;
    // With PreferCUVID, JPEG is still decoded by CUDA while video is decoded by NVDEC hardware
    videoDecodeCreateInfo.ulCreationFlags = cudaVideoCreate_PreferCUVID;
    videoDecodeCreateInfo.ulNumDecodeSurfaces = nDecodeSurface;
    videoDecodeCreateInfo.vidLock = m_ctxLock;
    videoDecodeCreateInfo.ulWidth = pVideoFormat->coded_width;
    videoDecodeCreateInfo.ulHeight = pVideoFormat->coded_height;
    // AV1 has max width/height of sequence in sequence header
    if (pVideoFormat->codec == cudaVideoCodec_AV1 && pVideoFormat->seqhdr_data_length > 0)
    {
        // dont overwrite if it is already set from cmdline or reconfig.txt
        if (!(m_nMaxWidth > pVideoFormat->coded_width || m_nMaxHeight > pVideoFormat->coded_height))
        {
            CUVIDEOFORMATEX *vidFormatEx = (CUVIDEOFORMATEX *)pVideoFormat;
            m_nMaxWidth = vidFormatEx->av1.max_width;
            m_nMaxHeight = vidFormatEx->av1.max_height;
        }
    }
    if (m_nMaxWidth < (int)pVideoFormat->coded_width)
        m_nMaxWidth = pVideoFormat->coded_width;
    if (m_nMaxHeight < (int)pVideoFormat->coded_height)
        m_nMaxHeight = pVideoFormat->coded_height;
    videoDecodeCreateInfo.ulMaxWidth = m_nMaxWidth;
    videoDecodeCreateInfo.ulMaxHeight = m_nMaxHeight;

    if (!(m_cropRect.r && m_cropRect.b) && !(m_resizeDim.w && m_resizeDim.h)) {
        m_nWidth = pVideoFormat->display_area.right - pVideoFormat->display_area.left;
        m_nLumaHeight = pVideoFormat->display_area.bottom - pVideoFormat->display_area.top;
        videoDecodeCreateInfo.ulTargetWidth = pVideoFormat->coded_width;
        videoDecodeCreateInfo.ulTargetHeight = pVideoFormat->coded_height;
    } else {
        if (m_resizeDim.w && m_resizeDim.h) {
            videoDecodeCreateInfo.display_area.left = pVideoFormat->display_area.left;
            videoDecodeCreateInfo.display_area.top = pVideoFormat->display_area.top;
            videoDecodeCreateInfo.display_area.right = pVideoFormat->display_area.right;
            videoDecodeCreateInfo.display_area.bottom = pVideoFormat->display_area.bottom;
            m_nWidth = m_resizeDim.w;
            m_nLumaHeight = m_resizeDim.h;
        }

        if (m_cropRect.r && m_cropRect.b) {
            videoDecodeCreateInfo.display_area.left = m_cropRect.l;
            videoDecodeCreateInfo.display_area.top = m_cropRect.t;
            videoDecodeCreateInfo.display_area.right = m_cropRect.r;
            videoDecodeCreateInfo.display_area.bottom = m_cropRect.b;
            m_nWidth = m_cropRect.r - m_cropRect.l;
            m_nLumaHeight = m_cropRect.b - m_cropRect.t;
        }
        videoDecodeCreateInfo.ulTargetWidth = m_nWidth;
        videoDecodeCreateInfo.ulTargetHeight = m_nLumaHeight;
    }

    m_nChromaHeight = (int)(ceil(m_nLumaHeight * GetChromaHeightFactor(m_eOutputFormat)));
    m_nNumChromaPlanes = GetChromaPlaneCount(m_eOutputFormat);
    m_nSurfaceHeight = videoDecodeCreateInfo.ulTargetHeight;
    m_nSurfaceWidth = videoDecodeCreateInfo.ulTargetWidth;
    m_displayRect.b = videoDecodeCreateInfo.display_area.bottom;
    m_displayRect.t = videoDecodeCreateInfo.display_area.top;
    m_displayRect.l = videoDecodeCreateInfo.display_area.left;
    m_displayRect.r = videoDecodeCreateInfo.display_area.right;

    m_videoInfo << "Video Decoding Params:" << std::endl
        << "\tNum Surfaces : " << videoDecodeCreateInfo.ulNumDecodeSurfaces << std::endl
        << "\tCrop         : [" << videoDecodeCreateInfo.display_area.left << ", " << videoDecodeCreateInfo.display_area.top << ", "
        << videoDecodeCreateInfo.display_area.right << ", " << videoDecodeCreateInfo.display_area.bottom << "]" << std::endl
        << "\tResize       : " << videoDecodeCreateInfo.ulTargetWidth << "x" << videoDecodeCreateInfo.ulTargetHeight << std::endl
        << "\tDeinterlace  : " << std::vector<const char *>{"Weave", "Bob", "Adaptive"}[videoDecodeCreateInfo.DeinterlaceMode]
    ;
    m_videoInfo << std::endl;

    m_frameQueue.configure(GetWidth() * m_nBPP, m_nLumaHeight + m_nChromaHeight * m_nNumChromaPlanes);

    CUDA_DRVAPI_CALL(NvidiaDriverApiProxy::instance().cuCtxPushCurrent(m_cuContext));
    NVDEC_API_CALL(NvidiaDriverDecoderProxy::instance().cuvidCreateDecoder(&m_hDecoder, &videoDecodeCreateInfo));
    CUDA_DRVAPI_CALL(NvidiaDriverApiProxy::instance().cuCtxPopCurrent(NULL));
    STOP_TIMER("Session Initialization Time: ");
    NvDecoder::addDecoderSessionOverHead(getDecoderSessionID(), elapsedTime);
    return nDecodeSurface;
}

int NvDecoder::ReconfigureDecoder(CUVIDEOFORMAT *pVideoFormat)
{
    if (pVideoFormat->bit_depth_luma_minus8 != m_videoFormat.bit_depth_luma_minus8 || pVideoFormat->bit_depth_chroma_minus8 != m_videoFormat.bit_depth_chroma_minus8){

        NVDEC_THROW_ERROR("Reconfigure Not supported for bit depth change", CUDA_ERROR_NOT_SUPPORTED);
    }

    if (pVideoFormat->chroma_format != m_videoFormat.chroma_format) {

        NVDEC_THROW_ERROR("Reconfigure Not supported for chroma format change", CUDA_ERROR_NOT_SUPPORTED);
    }

    bool bDecodeResChange = !(pVideoFormat->coded_width == m_videoFormat.coded_width && pVideoFormat->coded_height == m_videoFormat.coded_height);
    bool bDisplayRectChange = !(pVideoFormat->display_area.bottom == m_videoFormat.display_area.bottom && pVideoFormat->display_area.top == m_videoFormat.display_area.top \
        && pVideoFormat->display_area.left == m_videoFormat.display_area.left && pVideoFormat->display_area.right == m_videoFormat.display_area.right);

    int nDecodeSurface = pVideoFormat->min_num_decode_surfaces;

    if ((pVideoFormat->coded_width > m_nMaxWidth) || (pVideoFormat->coded_height > m_nMaxHeight)) {
        // For VP9, let driver  handle the change if new width/height > maxwidth/maxheight
        if ((m_eCodec != cudaVideoCodec_VP9) || m_bReconfigExternal)
        {
            NVDEC_THROW_ERROR("Reconfigure Not supported when width/height > maxwidth/maxheight", CUDA_ERROR_NOT_SUPPORTED);
        }
        return 1;
    }

    if (!bDecodeResChange && !m_bReconfigExtPPChange) {
        // if the coded_width/coded_height hasn't changed but display resolution has changed, then need to update width/height for
        // correct output without cropping. Example : 1920x1080 vs 1920x1088
        if (bDisplayRectChange)
        {
            m_nWidth = pVideoFormat->display_area.right - pVideoFormat->display_area.left;
            m_nLumaHeight = pVideoFormat->display_area.bottom - pVideoFormat->display_area.top;
            m_nChromaHeight = (int)ceil(m_nLumaHeight * GetChromaHeightFactor(m_eOutputFormat));
            m_nNumChromaPlanes = GetChromaPlaneCount(m_eOutputFormat);
        }

        // no need for reconfigureDecoder(). Just return
        return 1;
    }

    CUVIDRECONFIGUREDECODERINFO reconfigParams = { 0 };

    reconfigParams.ulWidth = m_videoFormat.coded_width = pVideoFormat->coded_width;
    reconfigParams.ulHeight = m_videoFormat.coded_height = pVideoFormat->coded_height;

    // Dont change display rect and get scaled output from decoder. This will help display app to present apps smoothly
    reconfigParams.display_area.bottom = m_displayRect.b;
    reconfigParams.display_area.top = m_displayRect.t;
    reconfigParams.display_area.left = m_displayRect.l;
    reconfigParams.display_area.right = m_displayRect.r;
    reconfigParams.ulTargetWidth = m_nSurfaceWidth;
    reconfigParams.ulTargetHeight = m_nSurfaceHeight;

    // If external reconfigure is called along with resolution change even if post processing params is not changed,
    // do full reconfigure params update
    if ((m_bReconfigExternal && bDecodeResChange) || m_bReconfigExtPPChange) {
        // update display rect and target resolution if requested explicitely
        m_bReconfigExternal = false;
        m_bReconfigExtPPChange = false;
        m_videoFormat = *pVideoFormat;
        if (!(m_cropRect.r && m_cropRect.b) && !(m_resizeDim.w && m_resizeDim.h)) {
            m_nWidth = pVideoFormat->display_area.right - pVideoFormat->display_area.left;
            m_nLumaHeight = pVideoFormat->display_area.bottom - pVideoFormat->display_area.top;
            reconfigParams.ulTargetWidth = pVideoFormat->coded_width;
            reconfigParams.ulTargetHeight = pVideoFormat->coded_height;
        }
        else {
            if (m_resizeDim.w && m_resizeDim.h) {
                reconfigParams.display_area.left = pVideoFormat->display_area.left;
                reconfigParams.display_area.top = pVideoFormat->display_area.top;
                reconfigParams.display_area.right = pVideoFormat->display_area.right;
                reconfigParams.display_area.bottom = pVideoFormat->display_area.bottom;
                m_nWidth = m_resizeDim.w;
                m_nLumaHeight = m_resizeDim.h;
            }

            if (m_cropRect.r && m_cropRect.b) {
                reconfigParams.display_area.left = m_cropRect.l;
                reconfigParams.display_area.top = m_cropRect.t;
                reconfigParams.display_area.right = m_cropRect.r;
                reconfigParams.display_area.bottom = m_cropRect.b;
                m_nWidth = m_cropRect.r - m_cropRect.l;
                m_nLumaHeight = m_cropRect.b - m_cropRect.t;
            }
            reconfigParams.ulTargetWidth = m_nWidth;
            reconfigParams.ulTargetHeight = m_nLumaHeight;
        }

        m_nChromaHeight = (int)ceil(m_nLumaHeight * GetChromaHeightFactor(m_eOutputFormat));
        m_nNumChromaPlanes = GetChromaPlaneCount(m_eOutputFormat);
        m_nSurfaceHeight = reconfigParams.ulTargetHeight;
        m_nSurfaceWidth = reconfigParams.ulTargetWidth;
        m_displayRect.b = reconfigParams.display_area.bottom;
        m_displayRect.t = reconfigParams.display_area.top;
        m_displayRect.l = reconfigParams.display_area.left;
        m_displayRect.r = reconfigParams.display_area.right;
    }

    reconfigParams.ulNumDecodeSurfaces = nDecodeSurface;

    START_TIMER
    CUDA_DRVAPI_CALL(NvidiaDriverApiProxy::instance().cuCtxPushCurrent(m_cuContext));
    NVDEC_API_CALL(NvidiaDriverDecoderProxy::instance().cuvidReconfigureDecoder(m_hDecoder, &reconfigParams));
    CUDA_DRVAPI_CALL(NvidiaDriverApiProxy::instance().cuCtxPopCurrent(NULL));
    STOP_TIMER("Session Reconfigure Time: ");

    return nDecodeSurface;
}

/* Return value from HandlePictureDecode() are interpreted as:
*  0: fail, >=1: succeeded
*/
int NvDecoder::HandlePictureDecode(CUVIDPICPARAMS *pPicParams) {
    if (m_checkOnlyMode)
        return 0;

    if (!m_hDecoder)
    {
        NVDEC_THROW_ERROR("Decoder not initialized.", CUDA_ERROR_NOT_INITIALIZED);
        return 0;
    }
    m_nPicNumInDecodeOrder[pPicParams->CurrPicIdx] = m_nDecodePicCnt++;
    CUDA_DRVAPI_CALL(NvidiaDriverApiProxy::instance().cuCtxPushCurrent(m_cuContext));
    NVDEC_API_CALL(NvidiaDriverDecoderProxy::instance().cuvidDecodePicture(m_hDecoder, pPicParams));
    if (m_bForce_zero_latency && ((!pPicParams->field_pic_flag) || (pPicParams->second_field)))
    {
        CUVIDPARSERDISPINFO dispInfo;
        memset(&dispInfo, 0, sizeof(dispInfo));
        dispInfo.picture_index = pPicParams->CurrPicIdx;
        dispInfo.progressive_frame = !pPicParams->field_pic_flag;
        dispInfo.top_field_first = pPicParams->bottom_field_flag ^ 1;
        HandlePictureDisplay(&dispInfo);
    }
    CUDA_DRVAPI_CALL(NvidiaDriverApiProxy::instance().cuCtxPopCurrent(NULL));
    return 1;
}

/* Return value from HandlePictureDisplay() are interpreted as:
*  0: fail, >=1: succeeded
*/
int NvDecoder::HandlePictureDisplay(CUVIDPARSERDISPINFO *pDispInfo) {
    CUVIDPROCPARAMS videoProcessingParameters = {};
    videoProcessingParameters.progressive_frame = pDispInfo->progressive_frame;
    videoProcessingParameters.second_field = pDispInfo->repeat_first_field + 1;
    videoProcessingParameters.top_field_first = pDispInfo->top_field_first;
    videoProcessingParameters.unpaired_field = pDispInfo->repeat_first_field < 0;
    videoProcessingParameters.output_stream = m_cuvidStream;

    CUdeviceptr dpSrcFrame = 0;
    unsigned int nSrcPitch = 0;
    CUDA_DRVAPI_CALL(NvidiaDriverApiProxy::instance().cuCtxPushCurrent(m_cuContext));


    uint8_t *pDecodedFrame = m_frameQueue.getFreeFrame();
    if (pDecodedFrame == nullptr)
    {
        NX_WARNING(this, "Not enought frame buffers!");
        throw NVDECException::makeNVDECException("Out of memory", CUDA_ERROR_OUT_OF_MEMORY, __FUNCTION__, __FILE__, __LINE__);
        return 1;
    }

    NVDEC_API_CALL(NvidiaDriverDecoderProxy::instance().cuvidMapVideoFrame(m_hDecoder, pDispInfo->picture_index, &dpSrcFrame,
        &nSrcPitch, &videoProcessingParameters));

    CUVIDGETDECODESTATUS DecodeStatus;
    memset(&DecodeStatus, 0, sizeof(DecodeStatus));
    CUresult result = NvidiaDriverDecoderProxy::instance().cuvidGetDecodeStatus(m_hDecoder, pDispInfo->picture_index, &DecodeStatus);
    if (result == CUDA_SUCCESS && (DecodeStatus.decodeStatus == cuvidDecodeStatus_Error || DecodeStatus.decodeStatus == cuvidDecodeStatus_Error_Concealed))
    {
        printf("Decode Error occurred for picture %d\n", m_nPicNumInDecodeOrder[pDispInfo->picture_index]);
    }

    // Copy luma plane
    CUDA_MEMCPY2D m = { 0 };
    m.srcMemoryType = CU_MEMORYTYPE_DEVICE;
    m.srcDevice = dpSrcFrame;
    m.srcPitch = nSrcPitch;
    m.dstMemoryType = m_bUseDeviceFrame ? CU_MEMORYTYPE_DEVICE : CU_MEMORYTYPE_HOST;
    m.dstDevice = (CUdeviceptr)(m.dstHost = pDecodedFrame);
    m.dstPitch = GetDeviceFramePitch();
    m.WidthInBytes = GetWidth() * m_nBPP;
    m.Height = m_nLumaHeight;
    CUDA_DRVAPI_CALL(NvidiaDriverApiProxy::instance().cuMemcpy2DAsync(&m, m_cuvidStream));

    // Copy chroma plane
    // NVDEC output has luma height aligned by 2. Adjust chroma offset by aligning height
    m.srcDevice = (CUdeviceptr)((uint8_t *)dpSrcFrame + m.srcPitch * ((m_nSurfaceHeight + 1) & ~1));
    m.dstDevice = (CUdeviceptr)(m.dstHost = pDecodedFrame + m.dstPitch * m_nLumaHeight);
    m.Height = m_nChromaHeight;
    CUDA_DRVAPI_CALL(NvidiaDriverApiProxy::instance().cuMemcpy2DAsync(&m, m_cuvidStream));

    if (m_nNumChromaPlanes == 2)
    {
        m.srcDevice = (CUdeviceptr)((uint8_t *)dpSrcFrame + m.srcPitch * ((m_nSurfaceHeight + 1) & ~1) * 2);
        m.dstDevice = (CUdeviceptr)(m.dstHost = pDecodedFrame + m.dstPitch * m_nLumaHeight * 2);
        m.Height = m_nChromaHeight;
        CUDA_DRVAPI_CALL(NvidiaDriverApiProxy::instance().cuMemcpy2DAsync(&m, m_cuvidStream));
    }
    CUDA_DRVAPI_CALL(NvidiaDriverApiProxy::instance().cuStreamSynchronize(m_cuvidStream));
    CUDA_DRVAPI_CALL(NvidiaDriverApiProxy::instance().cuCtxPopCurrent(NULL));

    m_timestamps.push_back(pDispInfo->timestamp);
    NVDEC_API_CALL(NvidiaDriverDecoderProxy::instance().cuvidUnmapVideoFrame(m_hDecoder, dpSrcFrame));
    return 1;
}

NvDecoder::NvDecoder(CUcontext cuContext, bool bUseDeviceFrame, cudaVideoCodec eCodec,
    bool checkOnlyMode, bool bLowLatency, const Rect *pCropRect, const Dim *pResizeDim,
    int maxWidth, int maxHeight, unsigned int clkRate, bool force_zero_latency) :
    m_cuContext(cuContext), m_checkOnlyMode(checkOnlyMode), m_bUseDeviceFrame(bUseDeviceFrame),
    m_eCodec(eCodec), m_nMaxWidth (maxWidth), m_nMaxHeight(maxHeight),
    m_bForce_zero_latency(force_zero_latency)
{
    if (!NvidiaDriverDecoderProxy::instance().load() || !NvidiaDriverApiProxy::instance().load())
        throw NVDECException::makeNVDECException("Failed to load driver library", CUDA_SUCCESS, __FUNCTION__, __FILE__, __LINE__);

    if (pCropRect) m_cropRect = *pCropRect;
    if (pResizeDim) m_resizeDim = *pResizeDim;

    NVDEC_API_CALL(NvidiaDriverDecoderProxy::instance().cuvidCtxLockCreate(&m_ctxLock, cuContext));

    decoderSessionID = 0;

    CUVIDPARSERPARAMS videoParserParameters = {};
    videoParserParameters.CodecType = eCodec;
    videoParserParameters.ulMaxNumDecodeSurfaces = 1;
    videoParserParameters.ulClockRate = clkRate;
    videoParserParameters.ulMaxDisplayDelay = bLowLatency ? 0 : 1;
    videoParserParameters.pUserData = this;
    videoParserParameters.pfnSequenceCallback = HandleVideoSequenceProc;
    videoParserParameters.pfnDecodePicture = HandlePictureDecodeProc;
    videoParserParameters.pfnDisplayPicture = m_bForce_zero_latency ? NULL : HandlePictureDisplayProc;
    videoParserParameters.pfnGetOperatingPoint = HandleOperatingPointProc;
    NVDEC_API_CALL(NvidiaDriverDecoderProxy::instance().cuvidCreateVideoParser(&m_hParser, &videoParserParameters));
}

NvDecoder::~NvDecoder() {

    START_TIMER

    if (m_hParser)
        NvidiaDriverDecoderProxy::instance().cuvidDestroyVideoParser(m_hParser);

    NvidiaDriverApiProxy::instance().cuCtxPushCurrent(m_cuContext);
    if (m_hDecoder)
        NvidiaDriverDecoderProxy::instance().cuvidDestroyDecoder(m_hDecoder);

    m_frameQueue.clear();
    NvidiaDriverApiProxy::instance().cuCtxPopCurrent(NULL);
    NvidiaDriverDecoderProxy::instance().cuvidCtxLockDestroy(m_ctxLock);

    STOP_TIMER("Session Deinitialization Time: ");

    NvDecoder::addDecoderSessionOverHead(getDecoderSessionID(), elapsedTime);
}

void  NvDecoder::Decode(const uint8_t *pData, int nSize, int nFlags, int64_t nTimestamp)
{
    CUVIDSOURCEDATAPACKET packet = { 0 };
    packet.payload = pData;
    packet.payload_size = nSize;
    packet.flags = nFlags | CUVID_PKT_TIMESTAMP;
    packet.timestamp = nTimestamp;
    if (!pData || nSize == 0) {
        packet.flags |= CUVID_PKT_ENDOFSTREAM;
    }
    NVDEC_API_CALL(NvidiaDriverDecoderProxy::instance().cuvidParseVideoData(m_hParser, &packet));
    m_cuvidStream = 0;
}

uint8_t* NvDecoder::GetFrame(int64_t* pTimestamp)
{
    auto frame = m_frameQueue.getNextFrame();
    if (frame && pTimestamp)
    {
        *pTimestamp = m_timestamps.front();
        m_timestamps.pop_front();
    }

    return frame;
}

void NvDecoder::releaseFrame(uint8_t* frame)
{
    m_frameQueue.releaseFrame(frame);
}
