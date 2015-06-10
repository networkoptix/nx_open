#ifndef _RPI_OMX_
#define _RPI_OMX_

#include <cstdio>
#include <vector>
#include <exception>
#include <atomic>

#define OMX
#define OMX_SKIP64BIT

#include "rpi_omx_broadcom.h"

#include <IL/OMX_Core.h>
#include <IL/OMX_Component.h>
#include <IL/OMX_Video.h>
#include <IL/OMX_Broadcom.h>

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
        :   errCode_(errCode)
        {
            setLastError(errCode);

            if (msg && msg[0])
                snprintf(msg_, MAX_LEN, "%s:%d OpenMAX IL error: 0x%08x. %s", file, line, errCode, msg);
            else
                snprintf(msg_, MAX_LEN, "%s:%d OpenMAX IL error: 0x%08x", file, line, errCode);
        }

        OMX_ERRORTYPE code() const { return errCode_; }
        virtual const char * what() const noexcept override { return msg_; }

        static bool hasError() { return lastError() || eventError(); }
        static uint32_t lastError() { return lastError_; }
        static uint32_t eventError() { return eventError_; }
        static void setLastError(uint32_t code = OMX_ErrorNone) { lastError_.store(code); }
        static void setEventError(uint32_t code = OMX_ErrorNone) { eventError_.store(code); }

    private:
        OMX_ERRORTYPE errCode_;
        char msg_[MAX_LEN];
        static std::atomic<uint32_t> lastError_;
        static std::atomic<uint32_t> eventError_;
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
            memset(&param_, 0, sizeof(param_));
            param_.nSize = sizeof(param_);
            param_.nVersion.nVersion = OMX_VERSION;
            param_.nVersion.s.nVersionMajor = OMX_VERSION_MAJOR;
            param_.nVersion.s.nVersionMinor = OMX_VERSION_MINOR;
            param_.nVersion.s.nRevision = OMX_VERSION_REVISION;
            param_.nVersion.s.nStep = OMX_VERSION_STEP;
        }

        T& operator * () { return param_; }
        T* operator & () { return &param_; }
        T* operator -> () { return &param_; }

        const T& operator * () const { return param_; }
        const T* operator & () const { return &param_; }
        const T* operator -> () const { return &param_; }

    private:
        T param_;
    };

    /// OpenMAX IL CPU<->GPU buffer
    class Buffer
    {
    public:
        Buffer()
        :   ppBuffer_(nullptr),
            fillDone_(false)
        {}

        bool filled() const
        {
            return fillDone_;
        }

        void setFilled(bool val = true)
        {
            Lock lock(VcosSemaphore::common()); // LOCK

            fillDone_ = val;
        }

        OMX_BUFFERHEADERTYPE ** pHeader() { return &ppBuffer_; }
        OMX_BUFFERHEADERTYPE * header() { return ppBuffer_; }
        OMX_U32 flags() const { return ppBuffer_->nFlags; }
        OMX_U32& flags() { return ppBuffer_->nFlags; }
        OMX_TICKS timeStamp() { return ppBuffer_->nTimeStamp; } // need OMX_IndexParamCommonUseStcTimestamps

        OMX_U8 * data() { return  ppBuffer_->pBuffer + ppBuffer_->nOffset; }
        OMX_U32 dataSize() const { return ppBuffer_->nFilledLen; }
        OMX_U32 allocSize() const { return ppBuffer_->nAllocLen; }

    private:
        OMX_BUFFERHEADERTYPE * ppBuffer_;
        std::atomic_bool fillDone_;
    };

    /// Base class for OpenMAX IL components
    class Component
    {
    public:
        OMX_HANDLETYPE& component() { return component_; }
        ComponentType type() const { return type_; }
        const char * name() const { return componentType2name(type_); }
        unsigned numPorts() const { return componentPortsCount(type_); }

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
                    OMX_ERRORTYPE err = OMX_GetParameter(component_, OMX_IndexParamVideoPortFormat, &portformat);
                    if (err != OMX_ErrorNone)
                        break;

                    dump_formats(&portformat);
                }
            }
        }

        OMX_STATETYPE state()
        {
            OMX_STATETYPE state;
            ERR_OMX( OMX_GetState(component_, &state), "OMX_GetState" );
            return state;
        }

        void switchState(OMX_STATETYPE newState)
        {
            unsigned value = eventState_ + 1;
            ERR_OMX( OMX_SendCommand(component_, OMX_CommandStateSet, newState, NULL), "switch state");

            if (! waitValue(&eventState_, value))
                debug_print("%s - lost state changed event\n", name());

            if (state() != newState)
                ERR_OMX( OMX_ErrorInvalidState, "can't switch state" );
        }

        unsigned count2wait(OMX_U32 nPortIndex) const { return (nPortIndex == OMX_ALL) ? numPorts() : 1; }

        void enablePort(OMX_U32 nPortIndex = OMX_ALL)
        {
            unsigned value = eventEnabled_ + count2wait(nPortIndex);
            ERR_OMX( OMX_SendCommand(component_, OMX_CommandPortEnable, nPortIndex, NULL), "enable port");

            if (! waitValue(&eventEnabled_, value))
                debug_print("%s - port %d lost enable port event\n", name(), nPortIndex);
        }

        void disablePort(OMX_U32 nPortIndex = OMX_ALL)
        {
            unsigned value = eventDisabled_ + count2wait(nPortIndex);
            ERR_OMX( OMX_SendCommand(component_, OMX_CommandPortDisable, nPortIndex, NULL), "disable port");

            if (! waitValue(&eventDisabled_, value))
                debug_print("%s - port %d lost disable port event\n", name(), nPortIndex);
        }

        void flushPort(OMX_U32 nPortIndex = OMX_ALL)
        {
            unsigned value = eventFlushed_ + count2wait(nPortIndex);
            ERR_OMX( OMX_SendCommand(component_, OMX_CommandFlush, nPortIndex, NULL), "flush buffers");

            if (! waitValue(&eventFlushed_, value))
                debug_print("%s - port %d lost flush port event\n", name(), nPortIndex);
        }

        void getPortDefinition(OMX_U32 nPortIndex, Parameter<OMX_PARAM_PORTDEFINITIONTYPE>& portDef)
        {
            portDef->nPortIndex = nPortIndex;
            ERR_OMX( OMX_GetParameter(component_, OMX_IndexParamPortDefinition, &portDef), "get port definition");
        }

        void setPortDefinition(OMX_U32 nPortIndex, Parameter<OMX_PARAM_PORTDEFINITIONTYPE>& portDef)
        {
            portDef->nPortIndex = nPortIndex;
            ERR_OMX( OMX_SetParameter(component_, OMX_IndexParamPortDefinition, &portDef), "set port definition");
        }

        void allocBuffers(OMX_U32 nPortIndex, Buffer& buffer)
        {
            Parameter<OMX_PARAM_PORTDEFINITIONTYPE> portDef;
            getPortDefinition(nPortIndex, portDef);

            ERR_OMX( OMX_AllocateBuffer(component_, buffer.pHeader(), nPortIndex, 0, portDef->nBufferSize), "OMX_AllocateBuffer");
        }

        void freeBuffers(OMX_U32 nPortIndex, Buffer& buffer)
        {
            ERR_OMX( OMX_FreeBuffer(component_, nPortIndex, buffer.header()), "OMX_FreeBuffer");
        }

        void callFillThisBuffer(Buffer& buffer)
        {
            ERR_OMX( OMX_FillThisBuffer(component_, buffer.header()), "OMX_FillThisBuffer");
        }

        void eventCmdComplete(OMX_U32 cmd, OMX_U32 /*nPortIndex*/)
        {
            Lock lock(VcosSemaphore::common());  // LOCK

            switch (cmd)
            {
                case OMX_CommandStateSet:
                    ++eventState_;
                    break;

                case OMX_CommandFlush:
                    ++eventFlushed_;
                    break;

                case OMX_CommandPortDisable:
                    ++eventDisabled_;
                    break;

                case OMX_CommandPortEnable:
                    ++eventEnabled_;
                    break;

                case OMX_CommandMarkBuffer:
                default:
                    break;
            }
        }

        void eventPortSettingsChanged(OMX_U32 /*nPortIndex*/)
        {
#if 0
            Lock lock(VcosSemaphore::common());  // LOCK

            ++changedPorts_[n2idx(nPortIndex)];
#endif
        }

    protected:
        OMX_HANDLETYPE component_;
        ComponentType type_;

        Component(ComponentType type, OMX_PTR pAppData, OMX_CALLBACKTYPE * callbacks)
        :   type_(type),
            eventState_(0),
            eventFlushed_(0),
            eventDisabled_(0),
            eventEnabled_(0)
        {
            //changedPorts_.resize(numPorts());

            OMX_STRING xName = const_cast<OMX_STRING>(name());
            ERR_OMX( OMX_GetHandle(&component_, xName, pAppData, callbacks), "OMX_GetHandle");

            disablePort();
        }

        ~Component()
        {
            try
            {
                ERR_OMX( OMX_FreeHandle(component_), "OMX_FreeHandle" );
            }
            catch (const OMXExeption& e)
            {
                OMXExeption::setLastError(e.code());
                debug_print("OMXExeption: OMX_FreeHandle(), %s", e.what());
            }
        }

        // type_ equals to first port number
        unsigned n2idx(OMX_U32 nPortIndex) const { return nPortIndex - type_; }
        unsigned idx2n(unsigned idx) const { return type_ + idx; }

    private:
        unsigned eventState_;
        unsigned eventFlushed_;
        unsigned eventDisabled_;
        unsigned eventEnabled_;
        //std::vector<unsigned> changedPorts_;

        bool waitValue(unsigned * pValue, unsigned wantedValue)
        {
            static const unsigned WAIT_CHANGES_US = 10000;
            static const unsigned MAX_WAIT_COUNT = 200;

            for (unsigned i = 0; i < MAX_WAIT_COUNT; ++i)
            {
                if (*pValue == wantedValue)
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
            ready_(false)
        {
            requestCallback();
            setDeviceNumber(CAM_DEVICE_NUMBER);

            Parameter<OMX_PARAM_TIMESTAMPMODETYPE> tsMode;
            tsMode->eTimestampMode = OMX_TimestampModeRawStc;

            ERR_OMX( OMX_SetConfig(component_, OMX_IndexParamCommonUseStcTimestamps, &tsMode), "camera timestamp mode");
        }

        void requestCallback()
        {
            Parameter<OMX_CONFIG_REQUESTCALLBACKTYPE> cbtype;
            cbtype->nPortIndex = OMX_ALL;
            cbtype->nIndex     = OMX_IndexParamCameraDeviceNumber;
            cbtype->bEnable    = OMX_TRUE;

            ERR_OMX( OMX_SetConfig(component_, OMX_IndexConfigRequestCallback, &cbtype), "request callbacks");
        }

        void setDeviceNumber(unsigned camNumber)
        {
            Parameter<OMX_PARAM_U32TYPE> device;
            device->nPortIndex = OMX_ALL;
            device->nU32 = camNumber;

            ERR_OMX( OMX_SetParameter(component_, OMX_IndexParamCameraDeviceNumber, &device), "set camera device number");
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

            ERR_OMX( OMX_SetConfig(component_, OMX_IndexConfigVideoFramerate, &framerate), "set framerate");
        }

        // -100 .. 100
        void setSharpness(OMX_U32 nPortIndex = OMX_ALL, OMX_S32 nSharpness = 0)
        {
            Parameter<OMX_CONFIG_SHARPNESSTYPE> sharpness;
            sharpness->nPortIndex = nPortIndex;
            sharpness->nSharpness = nSharpness;

            ERR_OMX( OMX_SetConfig(component_, OMX_IndexConfigCommonSharpness, &sharpness), "set camera sharpness");
        }

        // -100 .. 100
        void setContrast(OMX_U32 nPortIndex = OMX_ALL, OMX_S32 nContrast = 0)
        {
            Parameter<OMX_CONFIG_CONTRASTTYPE> contrast;
            contrast->nPortIndex = nPortIndex;
            contrast->nContrast = nContrast;

            ERR_OMX( OMX_SetConfig(component_, OMX_IndexConfigCommonContrast, &contrast), "set camera contrast");
        }

        // -100 .. 100
        void setSaturation(OMX_U32 nPortIndex = OMX_ALL, OMX_S32 nSaturation = 0)
        {
            Parameter<OMX_CONFIG_SATURATIONTYPE> saturation;
            saturation->nPortIndex = nPortIndex;
            saturation->nSaturation = nSaturation;

            ERR_OMX( OMX_SetConfig(component_, OMX_IndexConfigCommonSaturation, &saturation), "set camera saturation");
        }

        // 0 .. 100
        void setBrightness(OMX_U32 nPortIndex = OMX_ALL, OMX_U32 nBrightness = 50)
        {
            Parameter<OMX_CONFIG_BRIGHTNESSTYPE> brightness;
            brightness->nPortIndex = nPortIndex;
            brightness->nBrightness = nBrightness;

            ERR_OMX( OMX_SetConfig(component_, OMX_IndexConfigCommonBrightness, &brightness), "set camera brightness");
        }

        void setExposureValue(OMX_U32 nPortIndex = OMX_ALL,
                OMX_S32 xEVCompensation = 0, OMX_U32 nSensitivity = 100, OMX_BOOL bAutoSensitivity = OMX_FALSE)
        {
            Parameter<OMX_CONFIG_EXPOSUREVALUETYPE> exposure_value;
            exposure_value->nPortIndex = nPortIndex;
            exposure_value->xEVCompensation = xEVCompensation;
            exposure_value->nSensitivity = nSensitivity;
            exposure_value->bAutoSensitivity = bAutoSensitivity;

            ERR_OMX( OMX_SetConfig(component_, OMX_IndexConfigCommonExposureValue, &exposure_value), "set camera exposure value");
        }

        void setFrameStabilisation(OMX_U32 nPortIndex = OMX_ALL, OMX_BOOL bStab = OMX_TRUE)
        {
            Parameter<OMX_CONFIG_FRAMESTABTYPE> frame_stabilisation_control;
            frame_stabilisation_control->nPortIndex = nPortIndex;
            frame_stabilisation_control->bStab = bStab;

            ERR_OMX( OMX_SetConfig(component_, OMX_IndexConfigCommonFrameStabilisation, &frame_stabilisation_control),
                     "set camera frame stabilisation");
        }

        void setWhiteBalanceControl(OMX_U32 nPortIndex = OMX_ALL, OMX_WHITEBALCONTROLTYPE eWhiteBalControl = OMX_WhiteBalControlAuto)
        {
            Parameter<OMX_CONFIG_WHITEBALCONTROLTYPE> white_balance_control;
            white_balance_control->nPortIndex = nPortIndex;
            white_balance_control->eWhiteBalControl = eWhiteBalControl;

            ERR_OMX( OMX_SetConfig(component_, OMX_IndexConfigCommonWhiteBalance, &white_balance_control),
                     "set camera frame white balance");
        }

        void setImageFilter(OMX_U32 nPortIndex = OMX_ALL, OMX_IMAGEFILTERTYPE eImageFilter = OMX_ImageFilterNone)
        {
            Parameter<OMX_CONFIG_IMAGEFILTERTYPE> image_filter;
            image_filter->nPortIndex = nPortIndex;
            image_filter->eImageFilter = eImageFilter;

            ERR_OMX( OMX_SetConfig(component_, OMX_IndexConfigCommonImageFilter, &image_filter), "set camera image filter");
        }

        // OMX_MirrorHorizontal | OMX_MirrorVertical | OMX_MirrorBoth
        void setMirror(OMX_U32 nPortIndex, OMX_MIRRORTYPE eMirror = OMX_MirrorNone)
        {
            Parameter<OMX_CONFIG_MIRRORTYPE> mirror;
            mirror->nPortIndex = nPortIndex;
            mirror->eMirror = eMirror;

            ERR_OMX( OMX_SetConfig(component_, OMX_IndexConfigCommonMirror, &mirror), "set cammera mirror");
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

            ERR_OMX( OMX_SetParameter(component_, OMX_IndexConfigPortCapturing, &capture), "switch capture on port");
        }

        void allocBuffers()
        {
            Component::allocBuffers(IPORT, bufferIn_);
        }

        void freeBuffers()
        {
            Component::freeBuffers(IPORT, bufferIn_);
        }

        void eventReady()
        {
            Lock lock(VcosSemaphore::common()); // LOCK

            ready_ = true;
        }

        bool waitReady()
        {
            static const unsigned TIMEOUT_US = 100000;
            static const unsigned MAX_ATTEMPTS = 10;

            for (unsigned i = 0; i < MAX_ATTEMPTS; ++i)
            {
                if (ready_)
                    return true;
                usleep(TIMEOUT_US);
            }

            debug_print("lost ready event for Camera\n");
            return false;
        }

    private:
        Buffer bufferIn_;
        std::atomic_bool ready_;
    };

#if 0 // not used
    /// OMX.broadcom.clock
    class Clock : public Component
    {
    public:
        static const ComponentType cType = broadcom::CLOCK;

        static const unsigned OPORT_0 = 80;
        static const unsigned OPORT_1 = 81;
        static const unsigned OPORT_2 = 82;
        static const unsigned OPORT_3 = 83;
        static const unsigned OPORT_4 = 84;
        static const unsigned OPORT_5 = 85;

        Clock()
        :   Component(cType, (OMX_PTR) this, &cbsEvents)
        {
            Parameter<OMX_TIME_CONFIG_CLOCKSTATETYPE> clockState;
            clockState->eState = OMX_TIME_ClockStateRunning;
            //clockState->eState = OMX_TIME_ClockStateWaitingForStartTime;
            clockState->nWaitMask |= OMX_CLOCKPORT0;

            ERR_OMX( OMX_SetConfig(component_, OMX_IndexConfigTimeClockState, &clockState), "set clock state");
        }
    };
#endif

    /// OMX.broadcom.video_encode
    class Encoder : public Component
    {
    public:
        static const ComponentType cType = broadcom::VIDEO_ENCODER;

        static const unsigned IPORT = 200;
        static const unsigned OPORT = 201;

        Encoder()
        :   Component(cType, (OMX_PTR) this, &cbsEvents),
            empty_(true)
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

            ERR_OMX( OMX_SetParameter(component_, OMX_IndexParamVideoPortFormat, &format), "set video format");

            Parameter<OMX_CONFIG_PORTBOOLEANTYPE> ppsOption;
            ppsOption->nPortIndex = OPORT;
            ppsOption->bEnabled = OMX_TRUE;

            ERR_OMX( OMX_SetParameter(component_, OMX_IndexParamBrcmVideoAVCInlineHeaderEnable, &ppsOption), "enable SPS/PPS insertion");
        }

        void setBitrate(OMX_U32 bitrate, OMX_VIDEO_CONTROLRATETYPE type = OMX_Video_ControlRateVariable)
        {
            Parameter<OMX_VIDEO_PARAM_BITRATETYPE> brate;
            brate->nPortIndex = OPORT;
            brate->eControlRate = type;
            brate->nTargetBitrate = bitrate;

            ERR_OMX( OMX_SetParameter(component_, OMX_IndexParamVideoBitrate, &brate), "set bitrate");
        }

        void setIFramePeriod(unsigned value)
        {
            Parameter<OMX_PARAM_U32TYPE> period;
            period->nPortIndex = OPORT;
            period->nU32 = value;

            ERR_OMX( OMX_SetParameter(component_, OMX_IndexConfigBrcmVideoIntraPeriod, &period), "set IFrame period");
        }

        void allocBuffers()
        {
            Component::allocBuffers(OPORT, bufferOut_);
        }

        void freeBuffers()
        {
            empty_ = true;
            Component::freeBuffers(OPORT, bufferOut_);
        }

        void callFillThisBuffer()
        {
            Component::callFillThisBuffer(bufferOut_);
        }

        void prepare()
        {
            if (empty_)
            {
                callFillThisBuffer();
                empty_ = false;
            }
        }

        Buffer& outBuffer() { return bufferOut_; }

    private:
        Parameter<OMX_PARAM_PORTDEFINITIONTYPE> encoderPortDef_;
        Buffer bufferOut_;
        bool empty_;
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
