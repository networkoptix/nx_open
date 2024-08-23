// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <stdint.h>
#include <cstring>

#include <bcm_host.h>

#include "rpi_camera.h"

#define VERBOSE_DUMP_PORTS 0

using namespace rpi_omx;

namespace rpi_cam
{
    std::recursive_mutex RPiCamera::m_mutex;

    //

    void RPiCamera::init()
    {
        debug_print("VideoCore init()\n");

        bcm_host_init();

        try
        {
            ERR_OMX( OMX_Init(), "OMX_Init" );
        }
        catch (OMXExeption& ex)
        {
            debug_print("OMXExeption: %s %s\n", __FUNCTION__, ex.what());
        }
    }

    void RPiCamera::deinit()
    {
        debug_print("VideoCore deinit()\n");

        try
        {
            ERR_OMX( OMX_Deinit(), "OMX_Deinit" );
        }
        catch (OMXExeption& ex)
        {
            debug_print("OMXExeption: %s %s\n", __FUNCTION__, ex.what());
        }

        bcm_host_deinit();
    }

    RPiCamera::RPiCamera(const CameraParameters& camParams)
    :   m_needUpdate(false)
    {
        try
        {
            m_camParams = camParams;
            m_encParams[0].vfmt = camParams.vfmt;
            m_encParams[0].bitrateKbps = BITRATE_0();

            m_encParams[1].vfmt = getResizedFormat(camParams.vfmt);
            m_encParams[1].bitrateKbps = BITRATE_1();

            prepare(camParams, m_encParams[0], m_encParams[1]);
        }
        catch (const char * msg)
        {
            OMXExeption::setLastError(OMX_ErrorMax);
            debug_print("exception: %s %s", __FUNCTION__, msg);
        }
    }

    RPiCamera::~RPiCamera()
    {
        release();
    }

    bool RPiCamera::isOK() const
    {
        using namespace rpi_omx;

        if (OMXExeption::hasError())
        {
            debug_print("Camera is not OK: %s %s\n",
                        err2str((OMX_ERRORTYPE)OMXExeption::lastError()),
                        err2str((OMX_ERRORTYPE)OMXExeption::eventError()));
        }
        return ! rpi_omx::OMXExeption::hasError();
    }

    void RPiCamera::resetErrors()
    {
        OMXExeption::setLastError();
        OMXExeption::setEventError();
    }

    void RPiCamera::startCapture()
    {
        m_camera->capture(rpi_omx::Camera::OPORT_VIDEO, OMX_TRUE);
    }

    void RPiCamera::stopCapture()
    {
        m_camera->capture(rpi_omx::Camera::OPORT_VIDEO, OMX_FALSE);

#if 1   // It's not clear how to detect the moment when it's stopped capturing.
        static const unsigned TIME_TO_RELAX_US = 1000 * 1000;
        usleep(TIME_TO_RELAX_US);
#endif
    }

    void RPiCamera::prepare(const CameraParameters& camParams, const EncoderParameters& encParams0, const EncoderParameters& encParams1)
    {
        debug_print("%s\n", __FUNCTION__);

        std::lock_guard<std::recursive_mutex> lock(m_mutex); // LOCK

        try
        {
            m_camera.reset( new rpi_omx::Camera() );
            m_vsplitter.reset( new rpi_omx::VideoSplitter() );
            m_resizer.reset( new rpi_omx::Resizer() );
            m_encoders[0].reset( new rpi_omx::Encoder() );
            m_encoders[1].reset( new rpi_omx::Encoder() );
            if (! m_camera || ! m_vsplitter || ! m_resizer || ! m_encoders[0] || ! m_encoders[1])
                throw "can't create component";

            configCamera(camParams);
            configEncoders(encParams0, encParams1);

            setupTunnels();

            switchState(OMX_StateIdle);

            enablePorts();
            allocateBuffers();

            switchState(OMX_StateExecuting);

            startCapture();

            dumpPorts();
            m_needUpdate = false;
        }
        catch (const OMXExeption& ex)
        {
            debug_print("OMXExeption: %s %s\n", __FUNCTION__, ex.what());
        }
        catch (const char * msg)
        {
            OMXExeption::setLastError(OMX_ErrorMax);
            debug_print("exception: %s %s\n", __FUNCTION__, msg);
        }
    }

    void RPiCamera::release()
    {
        debug_print("%s\n", __FUNCTION__);

        std::lock_guard<std::recursive_mutex> lock(m_mutex); // LOCK

        try
        {
            bool softReset = m_camera && m_vsplitter && m_resizer && m_encoders[0] && m_encoders[1];
            if (softReset)
            {
                stopCapture();

                returnBuffers();
                flushBuffers();
                disablePorts();
                freeBuffers();

                teardownTunnels();

                switchState(OMX_StateIdle);
                switchState(OMX_StateLoaded);
            }

            m_camera.reset();
            m_vsplitter.reset();
            m_resizer.reset();
            m_encoders[0].reset();
            m_encoders[1].reset();

            resetErrors();
        }
        catch (const OMXExeption& ex)
        {
            debug_print("OMXExeption: %s %s\n", __FUNCTION__, ex.what());
        }
        catch (const char * msg)
        {
            OMXExeption::setLastError(OMX_ErrorMax);
            debug_print("exception: %s %s\n", __FUNCTION__, msg);
        }
    }

    void RPiCamera::configEncoder(unsigned streamNo, unsigned width, unsigned height, unsigned framerate, unsigned bitrateKbps)
    {
        debug_print("%s %d: %dx%dx%d %d kbps\n", __FUNCTION__, streamNo, width, height, framerate, bitrateKbps);

        if (streamNo)
            return; // first stream only

        std::lock_guard<std::recursive_mutex> lock(m_mutex); // LOCK

        if (m_encParams[0].vfmt.width != width)
        {
            m_encParams[0].vfmt.width = width;
            m_needUpdate = true;
        }

        if (m_encParams[0].vfmt.height != height)
        {
            m_encParams[0].vfmt.height = height;
            m_needUpdate = true;
        }

        if (framerate && m_encParams[0].vfmt.framerate != framerate)
        {
            m_encParams[0].vfmt.framerate = framerate;
            m_needUpdate = true;
        }

        if (bitrateKbps && m_encParams[0].bitrateKbps != bitrateKbps)
        {
            m_encParams[0].bitrateKbps = bitrateKbps;
            m_needUpdate = true;
        }

        if (m_needUpdate)
        {
            updateRatio(m_encParams[0].vfmt);
            m_encParams[1].vfmt = getResizedFormat(m_encParams[0].vfmt);
            m_camParams.vfmt = m_encParams[0].vfmt;
        }
    }

    void RPiCamera::update()
    {
        debug_print("%s\n", __FUNCTION__);

        rw_lock::ScopedLock<rw_lock::WRITE> scope(m_rw);

        std::lock_guard<std::recursive_mutex> lock(m_mutex); // LOCK

        try
        {
            if (m_needUpdate || !isOK())
            {
                release();
                prepare(m_camParams, m_encParams[0], m_encParams[1]);
            }
        }
        catch (const OMXExeption& ex)
        {
            debug_print("OMXExeption: %s %s\n", __FUNCTION__, ex.what());
        }
        catch (const char * msg)
        {
            OMXExeption::setLastError(OMX_ErrorMax);
            debug_print("exception, %s %s\n", __FUNCTION__, msg);
        }
    }

    /// @warning This method is called from separate threads (one per stream). So it can't lock common mutex.
    /// But it needs a guarantee, that another thread doesn't destroy components the same time.
    bool RPiCamera::read(unsigned streamNo, std::vector<uint8_t>& data, uint64_t& ts, unsigned& flags)
    {
        if (streamNo >= STREAMS_NUM())
            return false;

        rw_lock::ScopedLock<rw_lock::TRY_READ> scope(m_rw);
        if (!scope)
            return false;

        Encoder * encoder = m_encoders[streamNo].get();
        if (! encoder)
            return false;

        try
        {
            static const unsigned MAX_ATTEMPTS = 1000;

            encoder->prepare(); // fill buffers first time
            Buffer& encBuffer = encoder->outBuffer();

            for (unsigned i = 0; i < MAX_ATTEMPTS; ++i)
            {
                bool aval = false;
                if (encBuffer.filled())
                {
                    flags = encBuffer.flags();

                    unsigned bufSize = encBuffer.dataSize();
                    if (bufSize)
                    {
                        //debug_print("RPiCamera.read(). size: %d, flags: %d\n", bufSize, flags);

                        aval = true;
                        unsigned frameSize = data.size();
                        data.resize(frameSize + bufSize);

                        std::memcpy(&data[frameSize], encBuffer.data(), bufSize);

                        ts = encBuffer.timeStamp().nHighPart;
                        ts <<= 32;
                        ts |= encBuffer.timeStamp().nLowPart;
                    }

                    // Buffer flushed, request a new buffer to be filled by the encoder component
                    encBuffer.setFilled(false);
                    encoder->callFillThisBuffer();

                    if (flags & FLAG_CODECCONFIG)
                        continue; // merge SPS/PPS data with next frame

                    //debug_print("RPiCamera.read() timestamp: %llu\n", ts);

                    if (flags & FLAG_ENDOFFRAME)
                        return true;
                }

                if (!aval)
                    usleep(1000);
            }
        }
        catch (const OMXExeption& ex)
        {
            debug_print("OMXExeption: %s %s\n", __FUNCTION__, ex.what());
        }
        catch (const char * msg)
        {
            OMXExeption::setLastError(OMX_ErrorMax);
            debug_print("exception: %s %s\n", __FUNCTION__, msg);
        }

        debug_print("stop reading (timeout)\n");
        return false;
    }

    void RPiCamera::getEncoderConfig(unsigned streamNo, unsigned& width, unsigned& height, unsigned& fps, unsigned& bitrate) const
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex); // LOCK

        if (streamNo >= STREAMS_NUM())
            return;

        width = m_encParams[streamNo].vfmt.width;
        height = m_encParams[streamNo].vfmt.height;
        fps = m_encParams[streamNo].vfmt.framerate;
        bitrate = m_encParams[streamNo].bitrateKbps;
    }

    //

    void RPiCamera::configCamera(const CameraParameters& params)
    {
        m_camera->setVideoFromat(params.vfmt);

        m_camera->setImageDefaults();

        m_camera->setSharpness(OMX_ALL, params.sharpness);
        m_camera->setContrast(OMX_ALL, params.contrast);
        m_camera->setSaturation(OMX_ALL, params.saturation);
        m_camera->setBrightness(OMX_ALL, params.brightness);
        m_camera->setImageFilter(OMX_ALL, (OMX_IMAGEFILTERTYPE)params.filter);

        m_camera->waitReady();
    }

    void RPiCamera::configEncoders(const EncoderParameters& params0, const EncoderParameters& params1)
    {
        Parameter<OMX_PARAM_PORTDEFINITIONTYPE> portDef;
        m_camera->getPortDefinition(Camera::OPORT_VIDEO, portDef);

        // high
        m_encoders[0]->setupOutputPort(portDef, params0.bitrateKbps * 1000);
        m_encoders[0]->setCodec(OMX_VIDEO_CodingAVC);

        // resizer
        m_resizer->setupOutputPort(params1.vfmt.width, params1.vfmt.height);

        // low
        portDef->format.video.nFrameWidth = params1.vfmt.width;
        portDef->format.video.nFrameHeight = params1.vfmt.height;

        m_encoders[1]->setupOutputPort(portDef, params1.bitrateKbps * 1000);
        m_encoders[1]->setCodec(OMX_VIDEO_CodingAVC);
        m_encoders[1]->setIFramePeriod( IFRAME_PERIOD() );
    }

    void RPiCamera::setupTunnels()
    {
        ERR_OMX( OMX_SetupTunnel(m_camera->component(), Camera::OPORT_VIDEO, m_vsplitter->component(), VideoSplitter::IPORT),
            "setup tunnel camera.video -> splitter.input");

        ERR_OMX( OMX_SetupTunnel(m_vsplitter->component(), VideoSplitter::OPORT_1, m_encoders[0]->component(), Encoder::IPORT),
            "setup tunnel splitter.output -> encoder.input (high)");

        ERR_OMX( OMX_SetupTunnel(m_vsplitter->component(), VideoSplitter::OPORT_2, m_resizer->component(), Resizer::IPORT),
            "setup tunnel splitter.output -> resizer.input");

        ERR_OMX( OMX_SetupTunnel(m_resizer->component(), Resizer::OPORT, m_encoders[1]->component(), Encoder::IPORT),
            "setup tunnel resizer.output -> encoder.input (low)");
    }

    void RPiCamera::teardownTunnels()
    {
        ERR_OMX( OMX_SetupTunnel(m_camera->component(), Camera::OPORT_VIDEO, NULL, 0),
            "teardown tunnel camera.video -> splitter.input");

        ERR_OMX( OMX_SetupTunnel(m_vsplitter->component(), VideoSplitter::OPORT_1, NULL, 0),
            "teardown tunnel splitter.output -> encoder.input (high)");

        ERR_OMX( OMX_SetupTunnel(m_vsplitter->component(), VideoSplitter::OPORT_2, NULL, 0),
            "teardown tunnel splitter.output -> resizer.input");

        ERR_OMX( OMX_SetupTunnel(m_resizer->component(), Resizer::OPORT, NULL, 0),
            "teardown tunnel resizer.output -> encoder.input (low)");
    }

    void RPiCamera::switchState(OMX_STATETYPE state)
    {
        debug_print("%s %d\n", __FUNCTION__, state);

        m_camera->switchState(state);
        m_vsplitter->switchState(state);
        m_resizer->switchState(state);

        m_encoders[0]->switchState(state);
        m_encoders[1]->switchState(state);
    }

    void RPiCamera::enablePorts()
    {
        m_camera->enablePort(Camera::IPORT);
        m_camera->enablePort(Camera::OPORT_VIDEO);

        m_vsplitter->enablePort(VideoSplitter::IPORT);
        m_vsplitter->enablePort(VideoSplitter::OPORT_1);
        m_vsplitter->enablePort(VideoSplitter::OPORT_2);

        m_resizer->enablePort();
        m_encoders[0]->enablePort();
        m_encoders[1]->enablePort();
    }

    void RPiCamera::allocateBuffers()
    {
        m_camera->allocBuffers();
        m_encoders[0]->allocBuffers();
        m_encoders[1]->allocBuffers();
    }

    void RPiCamera::dumpPorts()
    {
        #if VERBOSE_DUMP_PORTS
            m_camera->dumpPort(Camera::IPORT);
            m_camera->dumpPort(Camera::OPORT_VIDEO);

            m_vsplitter->dumpPort(VideoSplitter::IPORT);
            m_vsplitter->dumpPort(VideoSplitter::OPORT_1);
            m_vsplitter->dumpPort(VideoSplitter::OPORT_2);

            m_resizer->dumpPort(Resizer::IPORT);
            m_resizer->dumpPort(Resizer::OPORT);

            m_encoders[0]->dumpPort(Encoder::IPORT);
            m_encoders[0]->dumpPort(Encoder::OPORT);

            m_encoders[1]->dumpPort(Encoder::IPORT);
            m_encoders[1]->dumpPort(Encoder::OPORT);
        #else
            m_camera->dumpPort(Camera::OPORT_VIDEO);
            m_encoders[0]->dumpPort(Encoder::OPORT);
            m_encoders[1]->dumpPort(Encoder::OPORT);
        #endif
    }

    // In fact I'm not sure if we realy need this step.
    void RPiCamera::returnBuffers()
    {
        m_encoders[0]->outBuffer().flags() |= OMX_BUFFERFLAG_EOS;
        m_encoders[1]->outBuffer().flags() |= OMX_BUFFERFLAG_EOS;

        m_encoders[0]->callFillThisBuffer();
        m_encoders[1]->callFillThisBuffer();
    }

    void RPiCamera::flushBuffers()
    {
        m_camera->flushPort(rpi_omx::Camera::OPORT_VIDEO);

        m_vsplitter->flushPort(rpi_omx::VideoSplitter::IPORT);
        m_vsplitter->flushPort(rpi_omx::VideoSplitter::OPORT_1);
        m_vsplitter->flushPort(rpi_omx::VideoSplitter::OPORT_2);

        m_resizer->flushPort();

        m_encoders[0]->flushPort();
        m_encoders[1]->flushPort();
    }

    void RPiCamera::disablePorts()
    {
        m_camera->disablePort(rpi_omx::Camera::OPORT_VIDEO);

        m_vsplitter->disablePort(rpi_omx::VideoSplitter::IPORT);
        m_vsplitter->disablePort(rpi_omx::VideoSplitter::OPORT_1);
        m_vsplitter->disablePort(rpi_omx::VideoSplitter::OPORT_2);

        m_resizer->disablePort();

        m_encoders[0]->disablePort();
        m_encoders[1]->disablePort();
    }

    void RPiCamera::freeBuffers()
    {
        m_camera->freeBuffers();

        m_encoders[0]->freeBuffers();
        m_encoders[1]->freeBuffers();
    }

    //

    const rpi_omx::VideoFromat * RPiCamera::getVideoFormats(unsigned& num, bool resized)
    {
        static const unsigned FORMATS_NUM = 3; //4;
        static const unsigned RESIZED_NUM = 2;
        static VideoFromat formats[FORMATS_NUM] = { /*VF_1920x1080x25,*/ VF_1280x960, VF_1280x720, VF_640x480 };
        static VideoFromat resizedFormats[RESIZED_NUM] = { VF_RESIZED_320x240, VF_RESIZED_320x180 };

        if (resized)
        {
            num = RESIZED_NUM;
            return resizedFormats;
        }

        num = FORMATS_NUM;
        return formats;
    }

    void RPiCamera::updateRatio(rpi_omx::VideoFromat& fmt)
    {
        unsigned num;
        const VideoFromat * formats = getVideoFormats(num);

        for (unsigned i = 0; i < num; ++i)
        {
            if (fmt.width == formats[i].width && fmt.height == formats[i].height)
            {
                fmt.ratio = formats[i].ratio;
                break;
            }
        }
    }

    const rpi_omx::VideoFromat& RPiCamera::getResizedFormat(const rpi_omx::VideoFromat& fmt)
    {
        if (fmt.ratio == VideoFromat::RATIO_16x9)
            return VF_RESIZED_320x180;
        return VF_RESIZED_320x240;
    }
}
