#include <cstdint>
#include <cstring>
//#include <ctime>

#include <bcm_host.h>

#include "rpi_camera.h"

using namespace rpi_omx;

namespace rpi_cam
{
    std::recursive_mutex RPiCamera::m_mutex;

    //

    void RPiCamera::init()
    {
        debug_print("RPiCamera::init()\n");

        bcm_host_init();

        try
        {
            ERR_OMX( OMX_Init(), "OMX_Init" );
        }
        catch (OMXExeption& ex)
        {
            debug_print("%s\n", ex.what());
        }

        rpi_omx::VcosSemaphore::common();
    }

    void RPiCamera::deinit()
    {
        debug_print("RPiCamera::deinit()\n");

        try
        {
            ERR_OMX( OMX_Deinit(), "OMX_Deinit" );
        }
        catch (OMXExeption& ex)
        {
            debug_print("%s\n", ex.what());
        }

        bcm_host_deinit();
    }

    void RPiCamera::resetComponents()
    {
        m_camera.reset();
        m_vsplitter.reset();
        m_resizer.reset();
        m_encoders[0].reset();
        m_encoders[1].reset();

        m_camera.reset( new rpi_omx::Camera() );
        m_vsplitter.reset( new rpi_omx::VideoSplitter() );
        m_resizer.reset( new rpi_omx::Resizer() );
        m_encoders[0].reset( new rpi_omx::Encoder() );
        m_encoders[1].reset( new rpi_omx::Encoder() );

        if (! m_camera || ! m_vsplitter || ! m_resizer || ! m_encoders[0] || ! m_encoders[1])
            throw "can't create component";
    }

    void RPiCamera::configCamera(const CameraParameters& params)
    {
        m_camera->setVideoFromat(params.vfmt);

        m_camera->setImageDefaults();

        m_camera->setSharpness(OMX_ALL, params.sharpness);
        m_camera->setContrast(OMX_ALL, params.contrast);
        m_camera->setSaturation(OMX_ALL, params.saturation);
        m_camera->setBrightness(OMX_ALL, params.brightness);
        m_camera->setImageFilter(OMX_ALL, (OMX_IMAGEFILTERTYPE)params.filter);

        for (unsigned i=0; i < 10; ++i)
        {
            if (m_camera->ready())
                break;

            debug_print("waiting for Camera component\n");
            usleep(100000);
        }
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
        debug_print("RPiCamera::switchState() %d\n", state);

#if 0
        if (state == OMX_StateIdle)
        {
            if (m_clock.state() == OMX_StateExecuting)
                m_clock.switchState(OMX_StatePause);
        }
        m_clock.switchState(state);
#endif
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
#if 0
        m_camera->dumpPort(Camera::IPORT, OMX_FALSE);
        m_camera->dumpPort(Camera::OPORT_VIDEO, OMX_FALSE);

        m_vsplitter->dumpPort(VideoSplitter::IPORT, OMX_FALSE);
        m_vsplitter->dumpPort(VideoSplitter::OPORT_1, OMX_FALSE);
        m_vsplitter->dumpPort(VideoSplitter::OPORT_2, OMX_FALSE);

        m_resizer->dumpPort(Resizer::IPORT);
        m_resizer->dumpPort(Resizer::OPORT);

        m_encoders[0]->dumpPort(Encoder::IPORT, OMX_FALSE);
        m_encoders[0]->dumpPort(Encoder::OPORT, OMX_FALSE);

        m_encoders[1]->dumpPort(Encoder::IPORT, OMX_FALSE);
        m_encoders[1]->dumpPort(Encoder::OPORT, OMX_FALSE);
#endif
        m_camera->dumpPort(Camera::OPORT_VIDEO, OMX_FALSE);
        m_encoders[0]->dumpPort(Encoder::OPORT, OMX_FALSE);
        m_encoders[1]->dumpPort(Encoder::OPORT, OMX_FALSE);
    }

    void RPiCamera::returnBuffers()
    {
        m_encoders[0]->outBuffer().flags() &= OMX_BUFFERFLAG_EOS;
        m_encoders[1]->outBuffer().flags() &= OMX_BUFFERFLAG_EOS;

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

        m_empty[1] = m_empty[0] = true;
    }

    void RPiCamera::prepare(const CameraParameters& camParams, const EncoderParameters& encParams0, const EncoderParameters& encParams1)
    {
        debug_print("RPiCamera::prepare()\n");

        std::lock_guard<std::recursive_mutex> lock(m_mutex); // LOCK

        m_isOK = false;

        try
        {
            configCamera(camParams);
            configEncoders(encParams0, encParams1);

            setupTunnels();

            switchState(OMX_StateIdle);

            enablePorts();
            allocateBuffers();

            switchState(OMX_StateExecuting);

            startCapture();

            dumpPorts();
            m_isOK = true;
        }
        catch (const OMXExeption& ex)
        {
            debug_print("exception RPiCamera::prepare(): %s\n", ex.what());
        }
        catch (const char * msg)
        {
            debug_print("exception RPiCamera::prepare(): %s\n", msg);
        }
    }

    void RPiCamera::release()
    {
        debug_print("RPiCamera::release()\n");

        std::lock_guard<std::recursive_mutex> lock(m_mutex); // LOCK

        m_isOK = false;

        try
        {
            stopCapture();

            returnBuffers();
            flushBuffers();
            disablePorts();
            freeBuffers();

            teardownTunnels();

            switchState(OMX_StateIdle);
            switchState(OMX_StateLoaded);

            m_camera.reset();
        }
        catch (const OMXExeption& ex)
        {
            debug_print("exception RPiCamera::release(): %s\n", ex.what());
        }
        catch (const char * msg)
        {
            debug_print("exception RPiCamera::release(): %s\n", msg);
        }
    }

    bool RPiCamera::read(rpi_omx::Encoder& encoder, std::vector<uint8_t>& data, uint64_t& ts, unsigned& flags)
    {
        try
        {
            static const unsigned MAX_ATTEMPTS = 1000;

            Buffer& encBuffer = encoder.outBuffer();

            for (unsigned i = 0; i < MAX_ATTEMPTS; ++i)
            {
                bool aval = false;
                if (encBuffer.filled())
                {
                    flags = encBuffer.flags();
#if 0
                    if (flags & FLAG_DATACORRUPT || flags & FLAG_EOS)
                    {
                        debug_print("RPiCamera.read() stop. Flags: %d\n", flags);
                        data.clear();
                        return false;
                    }
#endif
                    unsigned bufSize = encBuffer.dataSize();
                    if (bufSize)
                    {
                        //debug_print("RPiCamera.read(). size: %d, flags: %d\n", bufSize, flags);

                        aval = true;
                        unsigned frameSize = data.size();
                        data.resize(frameSize + bufSize);

                        std::memcpy(&data[frameSize], encBuffer.data(), bufSize);
#if 1
                        ts = encBuffer.timeStamp().nHighPart;
                        ts <<= 32;
                        ts |= encBuffer.timeStamp().nLowPart;
#else
                        struct timespec now;
                        clock_gettime(CLOCK_REALTIME, &now);
                        ts = now.tv_sec;
                        ts *= 1000 * 1000;
                        ts += now.tv_nsec / 1000;
#endif
                    }

                    // Buffer flushed, request a new buffer to be filled by the encoder component
                    encBuffer.setFilled(false);
                    encoder.callFillThisBuffer();

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
            debug_print("exception RPiCamera.read(): %s\n", ex.what());
        }
        catch (const char * msg)
        {
            debug_print("exception RPiCamera.read(): %s\n", msg);
        }

        //debug_print("RPiCamera.read() failed\n");
        return false;
    }

#if 0
    void RPiCamera::test()
    {
        rpi_omx::Encoder& encoder = m_encoders[0];
        FILE * outHigh = fopen("/home/pi/test.h264", "w+");

        Buffer& encBuffer = encoder.outBuffer();
        encoder.callFillThisBuffer();

        for (unsigned i = 0; i < 100; ++i)
        {
            bool aval = false;
            if (encBuffer.filled())
            {
                aval = true;

                // Flush buffer to output file
                unsigned toWrite = encBuffer.dataSize();
                if (toWrite)
                {
#if 1
                    size_t outWritten = fwrite(encBuffer.data(), 1, toWrite, outHigh);
                    if (outWritten != toWrite)
                    {
                        debug_print("Failed to write to output file: %s\n", strerror(errno));
                        break;
                    }
#endif
                    debug_print("[High] buffer -> out file: %d/%d flags: %d\n",
                           encBuffer.dataSize(), encBuffer.allocSize(), encBuffer.flags());
                }

                // Buffer flushed, request a new buffer to be filled by the encoder component
                encBuffer.setFilled(false);
                encoder.callFillThisBuffer();
            }

            if (!aval)
                usleep(1000);
        }

        if (outHigh)
            fclose(outHigh);
    }
#endif

    const rpi_omx::VideoFromat * RPiCamera::getVideoFormats(unsigned& num, bool resized)
    {
        static const unsigned FORMATS_NUM = 4;
        static const unsigned RESIZED_NUM = 2;
        static VideoFromat formats[FORMATS_NUM] = { VF_1920x1080, VF_1280x960, VF_1280x720, VF_640x480 };
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

    void RPiCamera::updateEncoder(unsigned width, unsigned height, unsigned framerate, unsigned bitrateKbps)
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex); // LOCK

        bool needUpdate = false;
        if (m_encParams[0].vfmt.width != width)
        {
            m_encParams[0].vfmt.width = width;
            needUpdate = true;
        }

        if (m_encParams[0].vfmt.height != height)
        {
            m_encParams[0].vfmt.height = height;
            needUpdate = true;
        }

        if (framerate && m_encParams[0].vfmt.framerate != framerate)
        {
            m_encParams[0].vfmt.framerate = framerate;
            needUpdate = true;
        }

        if (bitrateKbps && m_encParams[0].bitrateKbps != bitrateKbps)
        {
            m_encParams[0].bitrateKbps = bitrateKbps;
            needUpdate = true;
        }

        if (needUpdate)
        {
            updateRatio(m_encParams[0].vfmt);
            m_encParams[1].vfmt = getResizedFormat(m_encParams[0].vfmt);

            m_camParams.vfmt = m_encParams[0].vfmt;

            release();
            resetComponents();
            prepare(m_camParams, m_encParams[0], m_encParams[1]);
        }
    }
}
