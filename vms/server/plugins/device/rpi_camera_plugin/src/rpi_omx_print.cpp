// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rpi_omx.h"

namespace rpi_omx
{
    const char * err2str(OMX_ERRORTYPE error)
    {
        switch (error)
        {
            case OMX_ErrorNone:                                 return "OMX_ErrorNone";
            case OMX_ErrorInsufficientResources:                return "OMX_ErrorInsufficientResources";
            case OMX_ErrorUndefined:                            return "OMX_ErrorUndefined";
            case OMX_ErrorInvalidComponentName:                 return "OMX_ErrorInvalidComponentName";
            case OMX_ErrorComponentNotFound:                    return "OMX_ErrorComponentNotFound";
            case OMX_ErrorInvalidComponent:                     return "OMX_ErrorInvalidComponent";
            case OMX_ErrorBadParameter:                         return "OMX_ErrorBadParameter";
            case OMX_ErrorNotImplemented:                       return "OMX_ErrorNotImplemented";
            case OMX_ErrorUnderflow:                            return "OMX_ErrorUnderflow";
            case OMX_ErrorOverflow:                             return "OMX_ErrorOverflow";
            case OMX_ErrorHardware:                             return "OMX_ErrorHardware";
            case OMX_ErrorInvalidState:                         return "OMX_ErrorInvalidState";
            case OMX_ErrorStreamCorrupt:                        return "OMX_ErrorStreamCorrupt";
            case OMX_ErrorPortsNotCompatible:                   return "OMX_ErrorPortsNotCompatible";
            case OMX_ErrorResourcesLost:                        return "OMX_ErrorResourcesLost";
            case OMX_ErrorNoMore:                               return "OMX_ErrorNoMore";
            case OMX_ErrorVersionMismatch:                      return "OMX_ErrorVersionMismatch";
            case OMX_ErrorNotReady:                             return "OMX_ErrorNotReady";
            case OMX_ErrorTimeout:                              return "OMX_ErrorTimeout";
            case OMX_ErrorSameState:                            return "OMX_ErrorSameState";
            case OMX_ErrorResourcesPreempted:                   return "OMX_ErrorResourcesPreempted";
            case OMX_ErrorPortUnresponsiveDuringAllocation:     return "OMX_ErrorPortUnresponsiveDuringAllocation";
            case OMX_ErrorPortUnresponsiveDuringDeallocation:   return "OMX_ErrorPortUnresponsiveDuringDeallocation";
            case OMX_ErrorPortUnresponsiveDuringStop:           return "OMX_ErrorPortUnresponsiveDuringStop";
            case OMX_ErrorIncorrectStateTransition:             return "OMX_ErrorIncorrectStateTransition";
            case OMX_ErrorIncorrectStateOperation:              return "OMX_ErrorIncorrectStateOperation";
            case OMX_ErrorUnsupportedSetting:                   return "OMX_ErrorUnsupportedSetting";
            case OMX_ErrorUnsupportedIndex:                     return "OMX_ErrorUnsupportedIndex";
            case OMX_ErrorBadPortIndex:                         return "OMX_ErrorBadPortIndex";
            case OMX_ErrorPortUnpopulated:                      return "OMX_ErrorPortUnpopulated";
            case OMX_ErrorComponentSuspended:                   return "OMX_ErrorComponentSuspended";
            case OMX_ErrorDynamicResourcesUnavailable:          return "OMX_ErrorDynamicResourcesUnavailable";
            case OMX_ErrorMbErrorsInFrame:                      return "OMX_ErrorMbErrorsInFrame";
            case OMX_ErrorFormatNotDetected:                    return "OMX_ErrorFormatNotDetected";
            case OMX_ErrorContentPipeOpenFailed:                return "OMX_ErrorContentPipeOpenFailed";
            case OMX_ErrorContentPipeCreationFailed:            return "OMX_ErrorContentPipeCreationFailed";
            case OMX_ErrorSeperateTablesUsed:                   return "OMX_ErrorSeperateTablesUsed";
            case OMX_ErrorTunnelingUnsupported:                 return "OMX_ErrorTunnelingUnsupported";
            //
            case OMX_ErrorDiskFull:                             return "OMX_ErrorDiskFull";
            case OMX_ErrorMaxFileSize:                          return "OMX_ErrorMaxFileSize";
            case OMX_ErrorDrmUnauthorised:                      return "OMX_ErrorDrmUnauthorised";
            case OMX_ErrorDrmExpired:                           return "OMX_ErrorDrmExpired";
            case OMX_ErrorDrmGeneral:                           return "OMX_ErrorDrmGeneral";
            //
            case OMX_ErrorKhronosExtensions:
            case OMX_ErrorVendorStartUnused:
            case OMX_ErrorMax:
            //default: // commented: want warnings about others
                break;
        }

        return "";
    }

    static const char * format2str(OMX_VIDEO_CODINGTYPE c)
    {
        switch (c)
        {
            case OMX_VIDEO_CodingUnused:        return "not used";
            case OMX_VIDEO_CodingAutoDetect:    return "autodetect";
            case OMX_VIDEO_CodingMPEG2:         return "MPEG2";
            case OMX_VIDEO_CodingH263:          return "H.263";
            case OMX_VIDEO_CodingMPEG4:         return "MPEG4";
            case OMX_VIDEO_CodingWMV:           return "Windows Media Video";
            case OMX_VIDEO_CodingRV:            return "RealVideo";
            case OMX_VIDEO_CodingAVC:           return "H.264/AVC";
            case OMX_VIDEO_CodingMJPEG:         return "Motion JPEG";
            case OMX_VIDEO_CodingVP6:           return "VP6";
            case OMX_VIDEO_CodingVP7:           return "VP7";
            case OMX_VIDEO_CodingVP8:           return "VP8";
            case OMX_VIDEO_CodingYUV:           return "Raw YUV video";
            case OMX_VIDEO_CodingSorenson:      return "Sorenson";
            case OMX_VIDEO_CodingTheora:        return "OGG Theora";
            case OMX_VIDEO_CodingMVC:           return "H.264/MVC";
            default:
                debug_print("unknown OMX_VIDEO_CODINGTYPE: %d\n", c);
                return "unknown";
        }
    }

    static const char * format2str(OMX_COLOR_FORMATTYPE c)
    {
        switch (c)
        {
            case OMX_COLOR_FormatUnused:                 return "OMX_COLOR_FormatUnused: not used";
            case OMX_COLOR_FormatMonochrome:             return "OMX_COLOR_FormatMonochrome";
            case OMX_COLOR_Format8bitRGB332:             return "OMX_COLOR_Format8bitRGB332";
            case OMX_COLOR_Format12bitRGB444:            return "OMX_COLOR_Format12bitRGB444";
            case OMX_COLOR_Format16bitARGB4444:          return "OMX_COLOR_Format16bitARGB4444";
            case OMX_COLOR_Format16bitARGB1555:          return "OMX_COLOR_Format16bitARGB1555";
            case OMX_COLOR_Format16bitRGB565:            return "OMX_COLOR_Format16bitRGB565";
            case OMX_COLOR_Format16bitBGR565:            return "OMX_COLOR_Format16bitBGR565";
            case OMX_COLOR_Format18bitRGB666:            return "OMX_COLOR_Format18bitRGB666";
            case OMX_COLOR_Format18bitARGB1665:          return "OMX_COLOR_Format18bitARGB1665";
            case OMX_COLOR_Format19bitARGB1666:          return "OMX_COLOR_Format19bitARGB1666";
            case OMX_COLOR_Format24bitRGB888:            return "OMX_COLOR_Format24bitRGB888";
            case OMX_COLOR_Format24bitBGR888:            return "OMX_COLOR_Format24bitBGR888";
            case OMX_COLOR_Format24bitARGB1887:          return "OMX_COLOR_Format24bitARGB1887";
            case OMX_COLOR_Format25bitARGB1888:          return "OMX_COLOR_Format25bitARGB1888";
            case OMX_COLOR_Format32bitBGRA8888:          return "OMX_COLOR_Format32bitBGRA8888";
            case OMX_COLOR_Format32bitARGB8888:          return "OMX_COLOR_Format32bitARGB8888";
            case OMX_COLOR_FormatYUV411Planar:           return "OMX_COLOR_FormatYUV411Planar";
            case OMX_COLOR_FormatYUV411PackedPlanar:     return "OMX_COLOR_FormatYUV411PackedPlanar: Planes fragmented when a frame is split in multiple buffers";
            case OMX_COLOR_FormatYUV420Planar:           return "OMX_COLOR_FormatYUV420Planar: Planar YUV, 4:2:0 (I420)";
            case OMX_COLOR_FormatYUV420PackedPlanar:     return "OMX_COLOR_FormatYUV420PackedPlanar: Planar YUV, 4:2:0 (I420), planes fragmented when a frame is split in multiple buffers";
            case OMX_COLOR_FormatYUV420SemiPlanar:       return "OMX_COLOR_FormatYUV420SemiPlanar, Planar YUV, 4:2:0 (NV12), U and V planes interleaved with first U value";
            case OMX_COLOR_FormatYUV422Planar:           return "OMX_COLOR_FormatYUV422Planar";
            case OMX_COLOR_FormatYUV422PackedPlanar:     return "OMX_COLOR_FormatYUV422PackedPlanar: Planes fragmented when a frame is split in multiple buffers";
            case OMX_COLOR_FormatYUV422SemiPlanar:       return "OMX_COLOR_FormatYUV422SemiPlanar";
            case OMX_COLOR_FormatYCbYCr:                 return "OMX_COLOR_FormatYCbYCr";
            case OMX_COLOR_FormatYCrYCb:                 return "OMX_COLOR_FormatYCrYCb";
            case OMX_COLOR_FormatCbYCrY:                 return "OMX_COLOR_FormatCbYCrY";
            case OMX_COLOR_FormatCrYCbY:                 return "OMX_COLOR_FormatCrYCbY";
            case OMX_COLOR_FormatYUV444Interleaved:      return "OMX_COLOR_FormatYUV444Interleaved";
            case OMX_COLOR_FormatRawBayer8bit:           return "OMX_COLOR_FormatRawBayer8bit";
            case OMX_COLOR_FormatRawBayer10bit:          return "OMX_COLOR_FormatRawBayer10bit";
            case OMX_COLOR_FormatRawBayer8bitcompressed: return "OMX_COLOR_FormatRawBayer8bitcompressed";
            case OMX_COLOR_FormatL2:                     return "OMX_COLOR_FormatL2";
            case OMX_COLOR_FormatL4:                     return "OMX_COLOR_FormatL4";
            case OMX_COLOR_FormatL8:                     return "OMX_COLOR_FormatL8";
            case OMX_COLOR_FormatL16:                    return "OMX_COLOR_FormatL16";
            case OMX_COLOR_FormatL24:                    return "OMX_COLOR_FormatL24";
            case OMX_COLOR_FormatL32:                    return "OMX_COLOR_FormatL32";
            case OMX_COLOR_FormatYUV420PackedSemiPlanar: return "OMX_COLOR_FormatYUV420PackedSemiPlanar: Planar YUV, 4:2:0 (NV12), planes fragmented when a frame is split in multiple buffers, U and V planes interleaved with first U value";
            case OMX_COLOR_FormatYUV422PackedSemiPlanar: return "OMX_COLOR_FormatYUV422PackedSemiPlanar: Planes fragmented when a frame is split in multiple buffers";
            case OMX_COLOR_Format18BitBGR666:            return "OMX_COLOR_Format18BitBGR666";
            case OMX_COLOR_Format24BitARGB6666:          return "OMX_COLOR_Format24BitARGB6666";
            case OMX_COLOR_Format24BitABGR6666:          return "OMX_COLOR_Format24BitABGR6666";
            case OMX_COLOR_Format32bitABGR8888:          return "OMX_COLOR_Format32bitABGR8888";
            case OMX_COLOR_Format8bitPalette:            return "OMX_COLOR_Format8bitPalette";
            case OMX_COLOR_FormatYUVUV128:               return "OMX_COLOR_FormatYUVUV128";
            case OMX_COLOR_FormatRawBayer12bit:          return "OMX_COLOR_FormatRawBayer12bit";
            case OMX_COLOR_FormatBRCMEGL:                return "OMX_COLOR_FormatBRCMEGL";
            case OMX_COLOR_FormatBRCMOpaque:             return "OMX_COLOR_FormatBRCMOpaque";
            case OMX_COLOR_FormatYVU420PackedPlanar:     return "OMX_COLOR_FormatYVU420PackedPlanar";
            case OMX_COLOR_FormatYVU420PackedSemiPlanar: return "OMX_COLOR_FormatYVU420PackedSemiPlanar";
            default:
                debug_print("unknown OMX_COLOR_FORMATTYPE: %d\n", c);
                return "unknown";
        }
    }

    void dump_portdef(OMX_PARAM_PORTDEFINITIONTYPE* portdef)
    {
        debug_print("Port %d is %s, %s, buffers wants:%d needs:%d, size:%d, pop:%d, aligned:%d\n",
            portdef->nPortIndex,
            (portdef->eDir ==  OMX_DirInput ? "input" : "output"),
            (portdef->bEnabled == OMX_TRUE ? "enabled" : "disabled"),
            portdef->nBufferCountActual,
            portdef->nBufferCountMin,
            portdef->nBufferSize,
            portdef->bPopulated,
            portdef->nBufferAlignment);

        OMX_VIDEO_PORTDEFINITIONTYPE *viddef = &portdef->format.video;
        OMX_IMAGE_PORTDEFINITIONTYPE *imgdef = &portdef->format.image;

        switch (portdef->eDomain)
        {
            case OMX_PortDomainVideo:
                debug_print("Video type:\n"
                    "\tWidth:\t\t%d\n"
                    "\tHeight:\t\t%d\n"
                    "\tStride:\t\t%d\n"
                    "\tSliceHeight:\t%d\n"
                    "\tBitrate:\t%d\n"
                    "\tFramerate:\t%.02f\n"
                    "\tError hiding:\t%s\n"
                    "\tCodec:\t\t%s\n"
                    "\tColor:\t\t%s\n",
                    viddef->nFrameWidth,
                    viddef->nFrameHeight,
                    viddef->nStride,
                    viddef->nSliceHeight,
                    viddef->nBitrate,
                    ((float)viddef->xFramerate / (float)65536),
                    (viddef->bFlagErrorConcealment == OMX_TRUE ? "yes" : "no"),
                    format2str(viddef->eCompressionFormat),
                    format2str(viddef->eColorFormat));
                break;
            case OMX_PortDomainImage:
                debug_print("Image type:\n"
                    "\tWidth:\t\t%d\n"
                    "\tHeight:\t\t%d\n"
                    "\tStride:\t\t%d\n"
                    "\tSliceHeight:\t%d\n"
                    "\tError hiding:\t%s\n"
                    "\tCodec:\t\t%s\n"
                    "\tColor:\t\t%s\n",
                    imgdef->nFrameWidth,
                    imgdef->nFrameHeight,
                    imgdef->nStride,
                    imgdef->nSliceHeight,
                    (imgdef->bFlagErrorConcealment == OMX_TRUE ? "yes" : "no"),
                    format2str( (OMX_VIDEO_CODINGTYPE) imgdef->eCompressionFormat ),
                    format2str(imgdef->eColorFormat));
                break;
            default:
                break;
        }
    }

    void dump_formats(OMX_VIDEO_PARAM_PORTFORMATTYPE * portformat)
    {
        debug_print("\t%s, compression: %s\n", format2str(portformat->eColorFormat), format2str(portformat->eCompressionFormat));
    }

    const char * eventType2Str(OMX_EVENTTYPE eEvent)
    {
        switch (eEvent)
        {
            case OMX_EventCmdComplete:          return "OMX_EventCmdComplete";
            case OMX_EventError:                return "OMX_EventError";
            case OMX_EventMark:                 return "OMX_EventMark";
            case OMX_EventPortSettingsChanged:  return "OMX_EventPortSettingsChanged";
            case OMX_EventBufferFlag:           return "OMX_EventBufferFlag";
            case OMX_EventResourcesAcquired:    return "OMX_EventResourcesAcquired";
            case OMX_EventComponentResumed:     return "OMX_EventComponentResumed";
            case OMX_EventDynamicResourcesAvailable: return "OMX_EventDynamicResourcesAvailable";
            case OMX_EventPortFormatDetected:   return "OMX_EventPortFormatDetected";
            case OMX_EventKhronosExtensions:    return "OMX_EventKhronosExtensions";
            case OMX_EventVendorStartUnused:    return "OMX_EventVendorStartUnused";
            case OMX_EventParamOrConfigChanged: return "OMX_EventParamOrConfigChanged";
            default:
                break;
        };

        return nullptr;
    }

    void printEvent(const char * compName, OMX_HANDLETYPE hComponent, OMX_EVENTTYPE eEvent, OMX_U32 nData1, OMX_U32 nData2)
    {
        const char * strEvent = eventType2Str(eEvent);
        if (strEvent)
            debug_print("%s (%p) %s, data: %d, %d\n", compName, hComponent, strEvent, nData1, nData2);
        else
            debug_print("%s (%p) 0x%08x, data: %d, %d\n", compName, hComponent, eEvent, nData1, nData2);
    }
}
