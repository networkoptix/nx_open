// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef _RPI_OMX_
#define _RPI_OMX_

#include <atomic>
#include <cstdio>
#include <exception>
#include <vector>

#define OMX
#define OMX_SKIP64BIT

#include <IL/OMX_Broadcom.h>
#include <IL/OMX_Component.h>
#include <IL/OMX_Core.h>
#include <IL/OMX_Video.h>

#include "rpi_omx_broadcom.h"

#define debug_print(...) fprintf(stderr, "[RPiCamera] " __VA_ARGS__)
#define ERR_OMX(err, msg) if ((err) != OMX_ErrorNone) throw OMXExeption(err, __FILE__, __LINE__, msg)

/// Raspberry Pi OpenMAX IL layer
namespace rpi_omx
{
    typedef broadcom::ComponentType ComponentType;
    using broadcom::componentType2name;
    using broadcom::componentPortsCount;

    using broadcom::VcosSemaphore;
    using Lock = broadcom::VcosLock;

    const char * err2str(OMX_ERRORTYPE error);
    void printEvent(const char * compName, OMX_HANDLETYPE hComponent, OMX_EVENTTYPE eEvent, OMX_U32 nData1, OMX_U32 nData2);
    void dump_portdef(OMX_PARAM_PORTDEFINITIONTYPE * portdef);
    void dump_formats(OMX_VIDEO_PARAM_PORTFORMATTYPE * portformat);

    extern OMX_CALLBACKTYPE cbsEvents;

    /// Handler for OpenMAX IL errors
    class OMXExeption : public std::exception
    {
    public:
        static const unsigned MAX_LEN = 256;

        OMXExeption(OMX_ERRORTYPE errCode, const char * file, unsigned line, const char * msg = nullptr)
        :   m_errCode(errCode)
        {
            setLastError(errCode);

            if (msg && msg[0])
                snprintf(m_msg, MAX_LEN, "%s:%d OpenMAX IL error: 0x%08x. %s", file, line, errCode, msg);
            else
                snprintf(m_msg, MAX_LEN, "%s:%d OpenMAX IL error: 0x%08x", file, line, errCode);
        }

        OMX_ERRORTYPE code() const { return m_errCode; }
        virtual const char * what() const noexcept override { return m_msg; }

        static bool hasError() { return lastError() || eventError(); }
        static uint32_t lastError() { return m_lastError; }
        static uint32_t eventError() { return m_eventError; }
        static void setLastError(uint32_t code = OMX_ErrorNone) { m_lastError.store(code); }
        static void setEventError(uint32_t code = OMX_ErrorNone) { m_eventError.store(code); }

    private:
        OMX_ERRORTYPE m_errCode;
        char m_msg[MAX_LEN];
        static std::atomic<uint32_t> m_lastError;
        static std::atomic<uint32_t> m_eventError;
    };

    ///
    struct VideoFromat
    {
        typedef enum
        {
            RATIO_4x3,
            RATIO_16x9
        } Ratio;

        unsigned width;
        unsigned height;
        unsigned framerate;
        Ratio ratio;
        bool fov;
    };

    // resolution list from PiCam
    /// #   Resolution  Aspect Ratio    Framerates  Video   Image   FoV     Binning
    /// 1   1920x1080   16:9            1-30fps     x               Partial None
    /// 2   2592x1944   4:3             1-15fps     x       x       Full    None
    /// 3   2592x1944   4:3             0.1666-1fps x       x       Full    None
    /// 4   1296x972    4:3             1-42fps     x               Full    2x2
    /// 5   1296x730    16:9            1-49fps     x               Full    2x2
    /// 6   640x480     4:3             42.1-60fps  x               Full    4x4
    /// 7   640x480     4:3             60.1-90fps  x               Full    4x4
    ///
    static const VideoFromat VF_1920x1080 = { 1920, 1080, 30, VideoFromat::RATIO_16x9, false };
    static const VideoFromat VF_1920x1080x25 = { 1920, 1080, 25, VideoFromat::RATIO_16x9, false };
    static const VideoFromat VF_1280x960 = { 1280, 960, 30, VideoFromat::RATIO_4x3, true };
    static const VideoFromat VF_1280x720 = { 1280, 720, 30, VideoFromat::RATIO_16x9, true };
    static const VideoFromat VF_640x480 = { 640, 480, 30, VideoFromat::RATIO_4x3, true };

    static const VideoFromat VF_RESIZED_640x480 = VF_640x480;
    static const VideoFromat VF_RESIZED_320x240 = { 320, 240, 30, VideoFromat::RATIO_4x3, true };
    static const VideoFromat VF_RESIZED_256x192 = { 256, 192, 30, VideoFromat::RATIO_4x3, true };
    static const VideoFromat VF_RESIZED_160x120 = { 160, 120, 30, VideoFromat::RATIO_4x3, true };
    static const VideoFromat VF_RESIZED_128x96 = { 128, 96, 30, VideoFromat::RATIO_4x3, true };

    static const VideoFromat VF_RESIZED_960x540 = { 960, 540, 30, VideoFromat::RATIO_16x9, false };
    static const VideoFromat VF_RESIZED_640x360 = { 640, 360, 30, VideoFromat::RATIO_16x9, false };
    static const VideoFromat VF_RESIZED_480x270 = { 480, 270, 30, VideoFromat::RATIO_16x9, false };
    static const VideoFromat VF_RESIZED_384x216 = { 384, 216, 30, VideoFromat::RATIO_16x9, false };
    static const VideoFromat VF_RESIZED_320x180 = { 320, 180, 30, VideoFromat::RATIO_16x9, false };
    static const VideoFromat VF_RESIZED_240x135 = { 240, 135, 30, VideoFromat::RATIO_16x9, false };

    /// OpenMAX IL parameter
    template <typename T>
    class Parameter
    {
    public:
        Parameter()
        {
            init();
        }

        void init()
        {
            memset(&m_param, 0, sizeof(m_param));
            m_param.nSize = sizeof(m_param);
            m_param.nVersion.nVersion = OMX_VERSION;
            m_param.nVersion.s.nVersionMajor = OMX_VERSION_MAJOR;
            m_param.nVersion.s.nVersionMinor = OMX_VERSION_MINOR;
            m_param.nVersion.s.nRevision = OMX_VERSION_REVISION;
            m_param.nVersion.s.nStep = OMX_VERSION_STEP;
        }

        T& operator * () { return m_param; }
        T* operator & () { return &m_param; }
        T* operator -> () { return &m_param; }

        const T& operator * () const { return m_param; }
        const T* operator & () const { return &m_param; }
        const T* operator -> () const { return &m_param; }

    private:
        T m_param;
    };

    /// OpenMAX IL CPU<->GPU buffer
    class Buffer
    {
    public:
        Buffer()
        :   m_ppBuffer(nullptr),
            m_fillDone(false)
        {}

        bool filled() const { return m_fillDone; }
        void setFilled(bool val = true) { m_fillDone = val; }

        OMX_BUFFERHEADERTYPE ** pHeader() { return &m_ppBuffer; }
        OMX_BUFFERHEADERTYPE * header() { return m_ppBuffer; }
        OMX_U32 flags() const { return m_ppBuffer->nFlags; }
        OMX_U32& flags() { return m_ppBuffer->nFlags; }
        OMX_TICKS timeStamp() const { return m_ppBuffer->nTimeStamp; } // need OMX_IndexParamCommonUseStcTimestamps

        OMX_U8 * data() { return  m_ppBuffer->pBuffer + m_ppBuffer->nOffset; }
        OMX_U32 dataSize() const { return m_ppBuffer->nFilledLen; }
        OMX_U32 allocSize() const { return m_ppBuffer->nAllocLen; }

    private:
        OMX_BUFFERHEADERTYPE * m_ppBuffer;
        std::atomic_bool m_fillDone;
    };

    /// Base class for OpenMAX IL components
    class Component
    {
    public:
        OMX_HANDLETYPE& component() { return m_component; }
        ComponentType type() const { return m_type; }
        const char * name() const { return componentType2name(m_type); }
        unsigned numPorts() const { return componentPortsCount(m_type); }

        void dumpPort(OMX_U32 nPortIndex, bool dumpFormats = false)
        {
            Parameter<OMX_PARAM_PORTDEFINITIONTYPE> portdef;
            getPortDefinition(nPortIndex, portdef);

            dump_portdef(&portdef);

            if (dumpFormats)
            {
                Parameter<OMX_VIDEO_PARAM_PORTFORMATTYPE> portformat;
                portformat->nPortIndex = nPortIndex;
                portformat->nIndex = 0;

                for (;; portformat->nIndex++)
                {
                    OMX_ERRORTYPE err = OMX_GetParameter(m_component, OMX_IndexParamVideoPortFormat, &portformat);
                    if (err != OMX_ErrorNone)
                        break;

                    dump_formats(&portformat);
                }
            }
        }

        OMX_STATETYPE state()
        {
            OMX_STATETYPE state;
            ERR_OMX( OMX_GetState(m_component, &state), "OMX_GetState" );
            return state;
        }

        void switchState(OMX_STATETYPE newState)
        {
            unsigned value = m_eventState + 1;
            ERR_OMX( OMX_SendCommand(m_component, OMX_CommandStateSet, newState, NULL), "switch state");

            if (! waitValue(m_eventState, value))
                debug_print("%s - lost state changed event\n", name());

            if (state() != newState)
                ERR_OMX( OMX_ErrorInvalidState, "can't switch state" );
        }

        unsigned count2wait(OMX_U32 nPortIndex) const { return (nPortIndex == OMX_ALL) ? numPorts() : 1; }

        void enablePort(OMX_U32 nPortIndex = OMX_ALL)
        {
            unsigned value = m_eventEnabled + count2wait(nPortIndex);
            ERR_OMX( OMX_SendCommand(m_component, OMX_CommandPortEnable, nPortIndex, NULL), "enable port");

            if (! waitValue(m_eventEnabled, value))
                debug_print("%s - port %d lost enable port event\n", name(), nPortIndex);
        }

        void disablePort(OMX_U32 nPortIndex = OMX_ALL)
        {
            unsigned value = m_eventDisabled + count2wait(nPortIndex);
            ERR_OMX( OMX_SendCommand(m_component, OMX_CommandPortDisable, nPortIndex, NULL), "disable port");

            if (! waitValue(m_eventDisabled, value))
                debug_print("%s - port %d lost disable port event\n", name(), nPortIndex);
        }

        void flushPort(OMX_U32 nPortIndex = OMX_ALL)
        {
            unsigned value = m_eventFlushed + count2wait(nPortIndex);
            ERR_OMX( OMX_SendCommand(m_component, OMX_CommandFlush, nPortIndex, NULL), "flush buffers");

            if (! waitValue(m_eventFlushed, value))
                debug_print("%s - port %d lost flush port event\n", name(), nPortIndex);
        }

        void getPortDefinition(OMX_U32 nPortIndex, Parameter<OMX_PARAM_PORTDEFINITIONTYPE>& portDef)
        {
            portDef->nPortIndex = nPortIndex;
            ERR_OMX( OMX_GetParameter(m_component, OMX_IndexParamPortDefinition, &portDef), "get port definition");
        }

        void setPortDefinition(OMX_U32 nPortIndex, Parameter<OMX_PARAM_PORTDEFINITIONTYPE>& portDef)
        {
            portDef->nPortIndex = nPortIndex;
            ERR_OMX( OMX_SetParameter(m_component, OMX_IndexParamPortDefinition, &portDef), "set port definition");
        }

        void allocBuffers(OMX_U32 nPortIndex, Buffer& buffer)
        {
            Parameter<OMX_PARAM_PORTDEFINITIONTYPE> portDef;
            getPortDefinition(nPortIndex, portDef);

            ERR_OMX( OMX_AllocateBuffer(m_component, buffer.pHeader(), nPortIndex, 0, portDef->nBufferSize), "OMX_AllocateBuffer");
        }

        void freeBuffers(OMX_U32 nPortIndex, Buffer& buffer)
        {
            ERR_OMX( OMX_FreeBuffer(m_component, nPortIndex, buffer.header()), "OMX_FreeBuffer");
        }

        void callFillThisBuffer(Buffer& buffer)
        {
            ERR_OMX( OMX_FillThisBuffer(m_component, buffer.header()), "OMX_FillThisBuffer");
        }

        // from callbacks

        void eventCmdComplete(OMX_U32 cmd, OMX_U32 /*nPortIndex*/)
        {
            switch (cmd)
            {
                case OMX_CommandStateSet:
                    ++m_eventState;
                    break;

                case OMX_CommandFlush:
                    ++m_eventFlushed;
                    break;

                case OMX_CommandPortDisable:
                    ++m_eventDisabled;
                    break;

                case OMX_CommandPortEnable:
                    ++m_eventEnabled;
                    break;

                case OMX_CommandMarkBuffer:
                default:
                    break;
            }
        }

        void eventPortSettingsChanged(OMX_U32 /*nPortIndex*/)
        {
        }

        //VcosSemaphore * semaphore() { return &m_sem; }

    protected:
        OMX_HANDLETYPE m_component;
        ComponentType m_type;
        //VcosSemaphore m_sem; // commented: synced by atomics

        Component(ComponentType type, OMX_PTR pAppData, OMX_CALLBACKTYPE * callbacks)
        :   m_type(type),
            //m_sem("rpi_cam:event"),
            m_eventState(0),
            m_eventFlushed(0),
            m_eventDisabled(0),
            m_eventEnabled(0)
        {
            OMX_STRING xName = const_cast<OMX_STRING>(name());
            ERR_OMX( OMX_GetHandle(&m_component, xName, pAppData, callbacks), "OMX_GetHandle");

            disablePort();
        }

        ~Component()
        {
            try
            {
                ERR_OMX( OMX_FreeHandle(m_component), "OMX_FreeHandle" );
            }
            catch (const OMXExeption& e)
            {
                OMXExeption::setLastError(e.code());
                debug_print("OMXExeption: OMX_FreeHandle(), %s", e.what());
            }
        }

        // type_ equals to first port number
        unsigned n2idx(OMX_U32 nPortIndex) const { return nPortIndex - m_type; }
        unsigned idx2n(unsigned idx) const { return m_type + idx; }

    private:
        std::atomic<uint32_t> m_eventState;
        std::atomic<uint32_t> m_eventFlushed;
        std::atomic<uint32_t> m_eventDisabled;
        std::atomic<uint32_t> m_eventEnabled;

        bool waitValue(std::atomic<uint32_t>& value, unsigned wantedValue)
        {
            static const unsigned WAIT_CHANGES_US = 10000;
            static const unsigned MAX_WAIT_COUNT = 200;

            for (unsigned i = 0; i < MAX_WAIT_COUNT; ++i)
            {
                if (value >= wantedValue)
                    return true;

                usleep(WAIT_CHANGES_US);
            }

            return false;
        }
    };

    /// OMX.broadcom.camera
    class Camera : public Component
    {
    public:
        static const ComponentType cType = broadcom::CAMERA;

        static const unsigned OPORT_PREVIEW = 70;
        static const unsigned OPORT_VIDEO = 71;
        static const unsigned OPORT_STILL = 72;
        static const unsigned IPORT = 73;

        static const unsigned CAM_DEVICE_NUMBER = 0;

        static int32_t align(unsigned x, unsigned y)
        {
            return (x + y - 1) & (~(y - 1));
        }

        // The recommended initialisation sequence:
        // 1. Create component.
        // 2. Use OMX_IndexConfigRequestCallback to request callbacks on OMX_IndexParamCameraDeviceNumber.
        // 3. Set OMX_IndexParamISPTunerName.
        // 4. Set OMX_IndexParamCameraFlashType.
        // 5. Set OMX_IndexParamCameraDeviceNumber.
        // 6. Wait for the callback that OMX_IndexParamCameraDeviceNumber has changed.
        //      At this point, all the drivers have been loaded. Other settings can be applied whilst waiting for this event.
        // 7. Query for OMX_IndexConfigCameraSensorModes as required.
        // 8. Change state to IDLE, and proceed as required.
        Camera()
        :   Component(cType, (OMX_PTR) this, &cbsEvents),
            m_ready(false)
        {
            requestCallback();
            setDeviceNumber(CAM_DEVICE_NUMBER);

            Parameter<OMX_PARAM_TIMESTAMPMODETYPE> tsMode;
            tsMode->eTimestampMode = OMX_TimestampModeRawStc;

            ERR_OMX( OMX_SetConfig(m_component, OMX_IndexParamCommonUseStcTimestamps, &tsMode), "camera timestamp mode");
        }

        void requestCallback()
        {
            Parameter<OMX_CONFIG_REQUESTCALLBACKTYPE> cbtype;
            cbtype->nPortIndex = OMX_ALL;
            cbtype->nIndex     = OMX_IndexParamCameraDeviceNumber;
            cbtype->bEnable    = OMX_TRUE;

            ERR_OMX( OMX_SetConfig(m_component, OMX_IndexConfigRequestCallback, &cbtype), "request callbacks");
        }

        void setDeviceNumber(unsigned camNumber)
        {
            Parameter<OMX_PARAM_U32TYPE> device;
            device->nPortIndex = OMX_ALL;
            device->nU32 = camNumber;

            ERR_OMX( OMX_SetParameter(m_component, OMX_IndexParamCameraDeviceNumber, &device), "set camera device number");
        }

        void setVideoFromat(const VideoFromat& videoFormat)
        {
            Parameter<OMX_PARAM_PORTDEFINITIONTYPE> portDef;
            getPortDefinition(OPORT_VIDEO, portDef);

            portDef->format.video.nFrameWidth  = videoFormat.width;
            portDef->format.video.nFrameHeight = videoFormat.height;
            portDef->format.video.xFramerate   = videoFormat.framerate << 16;
            portDef->format.video.nStride      = align(portDef->format.video.nFrameWidth, portDef->nBufferAlignment);
            portDef->format.video.eColorFormat = OMX_COLOR_FormatYUV420PackedPlanar;

            setPortDefinition(OPORT_VIDEO, portDef);
            //setFramerate(videoFormat.framerate);
        }

        void setFramerate(unsigned fps)
        {
            Parameter<OMX_CONFIG_FRAMERATETYPE> framerate;
            framerate->xEncodeFramerate = fps << 16;
            framerate->nPortIndex = OPORT_VIDEO;

            ERR_OMX( OMX_SetConfig(m_component, OMX_IndexConfigVideoFramerate, &framerate), "set framerate");
        }

        // -100 .. 100
        void setSharpness(OMX_U32 nPortIndex = OMX_ALL, OMX_S32 nSharpness = 0)
        {
            Parameter<OMX_CONFIG_SHARPNESSTYPE> sharpness;
            sharpness->nPortIndex = nPortIndex;
            sharpness->nSharpness = nSharpness;

            ERR_OMX( OMX_SetConfig(m_component, OMX_IndexConfigCommonSharpness, &sharpness), "set camera sharpness");
        }

        // -100 .. 100
        void setContrast(OMX_U32 nPortIndex = OMX_ALL, OMX_S32 nContrast = 0)
        {
            Parameter<OMX_CONFIG_CONTRASTTYPE> contrast;
            contrast->nPortIndex = nPortIndex;
            contrast->nContrast = nContrast;

            ERR_OMX( OMX_SetConfig(m_component, OMX_IndexConfigCommonContrast, &contrast), "set camera contrast");
        }

        // -100 .. 100
        void setSaturation(OMX_U32 nPortIndex = OMX_ALL, OMX_S32 nSaturation = 0)
        {
            Parameter<OMX_CONFIG_SATURATIONTYPE> saturation;
            saturation->nPortIndex = nPortIndex;
            saturation->nSaturation = nSaturation;

            ERR_OMX( OMX_SetConfig(m_component, OMX_IndexConfigCommonSaturation, &saturation), "set camera saturation");
        }

        // 0 .. 100
        void setBrightness(OMX_U32 nPortIndex = OMX_ALL, OMX_U32 nBrightness = 50)
        {
            Parameter<OMX_CONFIG_BRIGHTNESSTYPE> brightness;
            brightness->nPortIndex = nPortIndex;
            brightness->nBrightness = nBrightness;

            ERR_OMX( OMX_SetConfig(m_component, OMX_IndexConfigCommonBrightness, &brightness), "set camera brightness");
        }

        void setExposureValue(OMX_U32 nPortIndex = OMX_ALL,
                OMX_S32 xEVCompensation = 0, OMX_U32 nSensitivity = 100, OMX_BOOL bAutoSensitivity = OMX_FALSE)
        {
            Parameter<OMX_CONFIG_EXPOSUREVALUETYPE> exposure_value;
            exposure_value->nPortIndex = nPortIndex;
            exposure_value->xEVCompensation = xEVCompensation;
            exposure_value->nSensitivity = nSensitivity;
            exposure_value->bAutoSensitivity = bAutoSensitivity;

            ERR_OMX( OMX_SetConfig(m_component, OMX_IndexConfigCommonExposureValue, &exposure_value), "set camera exposure value");
        }

        void setFrameStabilisation(OMX_U32 nPortIndex = OMX_ALL, OMX_BOOL bStab = OMX_TRUE)
        {
            Parameter<OMX_CONFIG_FRAMESTABTYPE> frame_stabilisation_control;
            frame_stabilisation_control->nPortIndex = nPortIndex;
            frame_stabilisation_control->bStab = bStab;

            ERR_OMX( OMX_SetConfig(m_component, OMX_IndexConfigCommonFrameStabilisation, &frame_stabilisation_control),
                     "set camera frame stabilisation");
        }

        void setWhiteBalanceControl(OMX_U32 nPortIndex = OMX_ALL, OMX_WHITEBALCONTROLTYPE eWhiteBalControl = OMX_WhiteBalControlAuto)
        {
            Parameter<OMX_CONFIG_WHITEBALCONTROLTYPE> white_balance_control;
            white_balance_control->nPortIndex = nPortIndex;
            white_balance_control->eWhiteBalControl = eWhiteBalControl;

            ERR_OMX( OMX_SetConfig(m_component, OMX_IndexConfigCommonWhiteBalance, &white_balance_control),
                     "set camera frame white balance");
        }

        void setImageFilter(OMX_U32 nPortIndex = OMX_ALL, OMX_IMAGEFILTERTYPE eImageFilter = OMX_ImageFilterNone)
        {
            Parameter<OMX_CONFIG_IMAGEFILTERTYPE> image_filter;
            image_filter->nPortIndex = nPortIndex;
            image_filter->eImageFilter = eImageFilter;

            ERR_OMX( OMX_SetConfig(m_component, OMX_IndexConfigCommonImageFilter, &image_filter), "set camera image filter");
        }

        // OMX_MirrorHorizontal | OMX_MirrorVertical | OMX_MirrorBoth
        void setMirror(OMX_U32 nPortIndex, OMX_MIRRORTYPE eMirror = OMX_MirrorNone)
        {
            Parameter<OMX_CONFIG_MIRRORTYPE> mirror;
            mirror->nPortIndex = nPortIndex;
            mirror->eMirror = eMirror;

            ERR_OMX( OMX_SetConfig(m_component, OMX_IndexConfigCommonMirror, &mirror), "set cammera mirror");
        }

        void setImageDefaults()
        {
            setSharpness();
            setContrast();
            setSaturation();
            setBrightness();
            setExposureValue();
            setFrameStabilisation();
            setWhiteBalanceControl();
            setImageFilter();
        }

        void capture(OMX_U32 nPortIndex, OMX_BOOL bEnabled)
        {
            Parameter<OMX_CONFIG_PORTBOOLEANTYPE> capture;
            capture->nPortIndex = nPortIndex;
            capture->bEnabled = bEnabled;

            ERR_OMX( OMX_SetParameter(m_component, OMX_IndexConfigPortCapturing, &capture), "switch capture on port");
        }

        void allocBuffers()
        {
            Component::allocBuffers(IPORT, m_bufferIn);
        }

        void freeBuffers()
        {
            Component::freeBuffers(IPORT, m_bufferIn);
        }

        void eventReady()
        {
            m_ready = true;
        }

        bool waitReady()
        {
            static const unsigned TIMEOUT_US = 100000;
            static const unsigned MAX_ATTEMPTS = 10;

            for (unsigned i = 0; i < MAX_ATTEMPTS; ++i)
            {
                if (m_ready)
                    return true;
                usleep(TIMEOUT_US);
            }

            debug_print("lost ready event for Camera\n");
            return false;
        }

    private:
        Buffer m_bufferIn;
        std::atomic_bool m_ready;
    };

    /// OMX.broadcom.video_encode
    class Encoder : public Component
    {
    public:
        static const ComponentType cType = broadcom::VIDEO_ENCODER;

        static const unsigned IPORT = 200;
        static const unsigned OPORT = 201;

        Encoder()
        :   Component(cType, (OMX_PTR) this, &cbsEvents),
            m_empty(true)
        {
        }

        void setupOutputPort(const Parameter<OMX_PARAM_PORTDEFINITIONTYPE>& cameraPortDef, unsigned bitrate, unsigned framerate = 0)
        {
            Parameter<OMX_PARAM_PORTDEFINITIONTYPE> portDef;
            getPortDefinition(OPORT, portDef);

            portDef->format.video.nFrameWidth  = cameraPortDef->format.video.nFrameWidth;
            portDef->format.video.nFrameHeight = cameraPortDef->format.video.nFrameHeight;
            portDef->format.video.xFramerate   = cameraPortDef->format.video.xFramerate;
            portDef->format.video.nStride      = cameraPortDef->format.video.nStride;
            portDef->format.video.nBitrate     = bitrate;

            if (framerate)
                portDef->format.video.xFramerate = framerate;

            setPortDefinition(OPORT, portDef);
        }

        void setCodec(OMX_VIDEO_CODINGTYPE codec)
        {
            Parameter<OMX_VIDEO_PARAM_PORTFORMATTYPE> format;
            format->nPortIndex = OPORT;
            format->eCompressionFormat = codec;

            ERR_OMX( OMX_SetParameter(m_component, OMX_IndexParamVideoPortFormat, &format), "set video format");

            Parameter<OMX_CONFIG_PORTBOOLEANTYPE> ppsOption;
            ppsOption->nPortIndex = OPORT;
            ppsOption->bEnabled = OMX_TRUE;

            ERR_OMX( OMX_SetParameter(m_component, OMX_IndexParamBrcmVideoAVCInlineHeaderEnable, &ppsOption), "enable SPS/PPS insertion");
        }

        void setBitrate(OMX_U32 bitrate, OMX_VIDEO_CONTROLRATETYPE type = OMX_Video_ControlRateVariable)
        {
            Parameter<OMX_VIDEO_PARAM_BITRATETYPE> brate;
            brate->nPortIndex = OPORT;
            brate->eControlRate = type;
            brate->nTargetBitrate = bitrate;

            ERR_OMX( OMX_SetParameter(m_component, OMX_IndexParamVideoBitrate, &brate), "set bitrate");
        }

        void setIFramePeriod(unsigned value)
        {
            Parameter<OMX_PARAM_U32TYPE> period;
            period->nPortIndex = OPORT;
            period->nU32 = value;

            ERR_OMX( OMX_SetParameter(m_component, OMX_IndexConfigBrcmVideoIntraPeriod, &period), "set IFrame period");
        }

        void allocBuffers()
        {
            Component::allocBuffers(OPORT, m_bufferOut);
        }

        void freeBuffers()
        {
            m_empty = true;
            Component::freeBuffers(OPORT, m_bufferOut);
        }

        void callFillThisBuffer()
        {
            Component::callFillThisBuffer(m_bufferOut);
        }

        void prepare()
        {
            if (m_empty)
            {
                callFillThisBuffer();
                m_empty = false;
            }
        }

        Buffer& outBuffer() { return m_bufferOut; }

    private:
        Parameter<OMX_PARAM_PORTDEFINITIONTYPE> m_encoderPortDef;
        Buffer m_bufferOut;
        bool m_empty;
    };

    /// OMX.broadcom.null_sink
    class NullSink : public Component
    {
    public:
        static const ComponentType cType = broadcom::NULL_SINK;

        static const unsigned IPORT = 240;

        NullSink()
        :   Component(cType, (OMX_PTR) this, &cbsEvents)
        {
        }
    };

    /// OMX.broadcom.video_splitter
    class VideoSplitter : public Component
    {
    public:
        static const ComponentType cType = broadcom::VIDEO_SPLITTER;

        static const unsigned IPORT = 250;
        static const unsigned OPORT_1 = 251;
        static const unsigned OPORT_2 = 252;
        static const unsigned OPORT_3 = 253;
        static const unsigned OPORT_4 = 254;

        VideoSplitter()
        :   Component(cType, (OMX_PTR) this, &cbsEvents)
        {
        }
    };

    /// OMX.broadcom.resize
    class Resizer : public Component
    {
    public:
        static const ComponentType cType = broadcom::RESIZER;

        static const unsigned IPORT = 60;
        static const unsigned OPORT = 61;

        Resizer()
        :   Component(cType, (OMX_PTR) this, &cbsEvents)
        {
        }

        void setupOutputPort(unsigned newWidth, unsigned newHeight)
        {
            Parameter<OMX_PARAM_PORTDEFINITIONTYPE> portDef;
            getPortDefinition(IPORT, portDef);

            portDef->format.image.nFrameWidth = newWidth;
            portDef->format.image.nFrameHeight = newHeight;
            portDef->format.image.eColorFormat = OMX_COLOR_FormatYUV420PackedPlanar;

            portDef->format.image.nSliceHeight = 0;
            portDef->format.image.nStride = 0;

            setPortDefinition(OPORT, portDef);
        }
    };
}

#endif
