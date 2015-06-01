#include <cstdint>
#include <cstring>
//#include <ctime>

#include <bcm_host.h>

#include "rpi_camera.h"

using namespace rpi_omx;

namespace rpi_cam
{
    void RPiCamera::init()
    {
        printf("[camera] RPiCamera::init()\n");

        bcm_host_init();

        try
        {
            ERR_OMX( OMX_Init(), "OMX_Init" );
        }
        catch (OMXExeption& ex)
        {
            printf("[camera] %s\n", ex.what());
        }

        rpi_omx::VcosSemaphore::common();
    }

    void RPiCamera::deinit()
    {
        printf("[camera] RPiCamera::deinit()\n");

        try
        {
            ERR_OMX( OMX_Deinit(), "OMX_Deinit" );
        }
        catch (OMXExeption& ex)
        {
            printf("[camera] %s\n", ex.what());
        }

        bcm_host_deinit();
    }

    void RPiCamera::configCamera(const CameraParameters& params)
    {
        VideoFromat vf = m_vfs[0];
        vf.framerate = m_fps;

        m_camera.setVideoFromat(vf);

        m_camera.setImageDefaults();
        //m_camera.setImageFilter(OMX_ALL, OMX_ImageFilterNoise);

        m_camera.setSharpness(OMX_ALL, params.sharpness);
        m_camera.setContrast(OMX_ALL, params.contrast);
        m_camera.setSaturation(OMX_ALL, params.saturation);
        m_camera.setBrightness(OMX_ALL, params.brightness);
        m_camera.setImageFilter(OMX_ALL, (OMX_IMAGEFILTERTYPE)params.filter);

        while (!m_camera.ready())
        {
            printf("[camera] waiting for Camera component\n");
            usleep(10000);
        }
    }

    void RPiCamera::configEncoders()
    {
        Parameter<OMX_PARAM_PORTDEFINITIONTYPE> portDef;
        m_camera.getPortDefinition(Camera::OPORT_VIDEO, portDef);

        // high
        m_encoders[0].setupOutputPort(portDef, m_bitrates[0] * 1000);
        m_encoders[0].setCodec(OMX_VIDEO_CodingAVC);

        // resizer
        m_resizer.setupOutputPort(m_vfs[1].width, m_vfs[1].height);

        // low
        portDef->format.video.nFrameWidth = m_vfs[1].width;
        portDef->format.video.nFrameHeight = m_vfs[1].height;

        m_encoders[1].setupOutputPort(portDef, m_bitrates[1] * 1000);
        m_encoders[1].setCodec(OMX_VIDEO_CodingAVC);

        m_encoders[1].setIFramePeriod( IFRAME_PERIOD() );
    }

    void RPiCamera::tunnelPorts()
    {
        ERR_OMX( OMX_SetupTunnel(m_camera.component(), Camera::OPORT_VIDEO, m_vsplitter.component(), VideoSplitter::IPORT),
            "tunnel camera.video -> splitter.input");

        ERR_OMX( OMX_SetupTunnel(m_vsplitter.component(), VideoSplitter::OPORT_1, m_encoders[0].component(), Encoder::IPORT),
            "tunnel splitter.output -> encoder.input (high)");

        ERR_OMX( OMX_SetupTunnel(m_vsplitter.component(), VideoSplitter::OPORT_2, m_resizer.component(), Resizer::IPORT),
            "tunnel splitter.output -> resizer.input");

        ERR_OMX( OMX_SetupTunnel(m_resizer.component(), Resizer::OPORT, m_encoders[1].component(), Encoder::IPORT),
            "tunnel resizer.output -> encoder.input (low)");
    }

    void RPiCamera::switchState(OMX_STATETYPE state)
    {
#if 0
        if (state == OMX_StateIdle)
        {
            if (m_clock.state() == OMX_StateExecuting)
                m_clock.switchState(OMX_StatePause);
        }
        m_clock.switchState(state);
#endif
        m_camera.switchState(state);
        m_vsplitter.switchState(state);
        m_resizer.switchState(state);

        m_encoders[0].switchState(state);
        m_encoders[1].switchState(state);
    }

    void RPiCamera::enablePorts()
    {
        m_camera.enablePort(Camera::IPORT);
        m_camera.enablePort(Camera::OPORT_VIDEO);

        m_vsplitter.enablePort(VideoSplitter::IPORT);
        m_vsplitter.enablePort(VideoSplitter::OPORT_1);
        m_vsplitter.enablePort(VideoSplitter::OPORT_2);

        m_resizer.enablePort();
        m_encoders[0].enablePort();
        m_encoders[1].enablePort();
    }

    void RPiCamera::allocateBuffers()
    {
        m_camera.allocBuffers();
        m_encoders[0].allocBuffers();
        m_encoders[1].allocBuffers();
    }

    void RPiCamera::dumpPorts()
    {
        m_camera.dumpPort(Camera::IPORT, OMX_FALSE);
        m_camera.dumpPort(Camera::OPORT_VIDEO, OMX_FALSE);

        m_vsplitter.dumpPort(VideoSplitter::IPORT, OMX_FALSE);
        m_vsplitter.dumpPort(VideoSplitter::OPORT_1, OMX_FALSE);
        m_vsplitter.dumpPort(VideoSplitter::OPORT_2, OMX_FALSE);

        m_resizer.dumpPort(Resizer::IPORT);
        m_resizer.dumpPort(Resizer::OPORT);

        m_encoders[0].dumpPort(Encoder::IPORT, OMX_FALSE);
        m_encoders[0].dumpPort(Encoder::OPORT, OMX_FALSE);

        m_encoders[1].dumpPort(Encoder::IPORT, OMX_FALSE);
        m_encoders[1].dumpPort(Encoder::OPORT, OMX_FALSE);
    }

    void RPiCamera::returnBuffers()
    {
#if 0
        m_encoders[0].outBuffer().flags() &= OMX_BUFFERFLAG_EOS;
        m_encoders[1].outBuffer().flags() &= OMX_BUFFERFLAG_EOS;
#endif
        m_encoders[0].callFillThisBuffer();
        m_encoders[1].callFillThisBuffer();
    }

    void RPiCamera::flushBuffers()
    {
        m_camera.flushPort();

        m_vsplitter.flushPort();
        m_resizer.flushPort();

        m_encoders[0].flushPort();
        m_encoders[1].flushPort();
    }

    void RPiCamera::disablePorts()
    {
        m_camera.disablePort();

        m_vsplitter.disablePort();
        m_resizer.disablePort();

        m_encoders[0].disablePort();
        m_encoders[1].disablePort();
    }

    void RPiCamera::freeBuffers()
    {
        m_camera.freeBuffers();

        m_encoders[0].freeBuffers();
        m_encoders[1].freeBuffers();
    }

    //

    bool RPiCamera::config(unsigned mode, unsigned fps, unsigned bitrate0, unsigned bitrate1)
    {
        switch (mode)
        {
            case Resolutions_default: // do not change
                break;

            case Resolutions_1280x960_640x480:
                m_vfs[0] = VF_1280x960;
                m_vfs[1] = VF_RESIZED_640x480;
                break;

            case Resolutions_1280x960_320x240:
                m_vfs[0] = VF_1280x960;
                m_vfs[1] = VF_RESIZED_320x240;
                break;

            case Resolutions_1920x1080_640x360:
                m_vfs[0] = VF_1920x1080;
                m_vfs[1] = VF_RESIZED_640x360;
                break;

            case Resolutions_1920x1080_480x270:
                m_vfs[0] = VF_1920x1080;
                m_vfs[1] = VF_RESIZED_480x270;
                break;

            case Resolutions_1920x1080_320x180:
                m_vfs[0] = VF_1920x1080;
                m_vfs[1] = VF_RESIZED_320x180;
                break;

            default:
                return false;
        }

        if (fps)
            m_fps = fps;
        if (! m_fps || m_fps > m_vfs[0].framerate)
            m_fps = m_vfs[0].framerate;

        if (bitrate0)
            m_bitrates[0] = bitrate0;
        if (bitrate1)
            m_bitrates[1] = bitrate1;

        return true;
    }

    void RPiCamera::getConfig(unsigned streamNo, unsigned& width, unsigned& height, unsigned& fps, unsigned& bitrate) const
    {
        if (streamNo >= STREAMS_NUM())
            return;

        width = m_vfs[streamNo].width;
        height = m_vfs[streamNo].height;
        fps = m_fps;
        bitrate = m_bitrates[streamNo];
    }

    bool RPiCamera::prepare(const CameraParameters& params)
    {
        try
        {
            configCamera(params);
            configEncoders();
            tunnelPorts();

            switchState(OMX_StateIdle);

            enablePorts();
            allocateBuffers();

            switchState(OMX_StateExecuting);

            startCapture();
#if 0
            dumpPorts();
#endif
            return true;
        }
        catch (const OMXExeption& ex)
        {
            printf("[camera] exception RPiCamera::prepare(): %s\n", ex.what());
        }
        catch (const char * msg)
        {
            printf("[camera] exception RPiCamera::prepare(): %s\n", msg);
        }

        return false;
    }

    bool RPiCamera::release()
    {
        try
        {
            stopCapture();

            returnBuffers();
            flushBuffers();
            disablePorts();
            freeBuffers();

            switchState(OMX_StateIdle);
            switchState(OMX_StateLoaded);
            return true;
        }
        catch (const OMXExeption& ex)
        {
            printf("[camera] exception RPiCamera::release(): %s\n", ex.what());
        }
        catch (const char * msg)
        {
            printf("[camera] exception RPiCamera::release(): %s\n", msg);
        }

        return false;
    }

    void RPiCamera::fillBuffer(unsigned streamNo)
    {
        if (streamNo < STREAMS_NUM())
            m_encoders[streamNo].callFillThisBuffer();
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
                        printf("[camera] RPiCamera.read() stop. Flags: %d\n", flags);
                        data.clear();
                        return false;
                    }
#endif
                    unsigned bufSize = encBuffer.dataSize();
                    if (bufSize)
                    {
                        //printf("[camera] RPiCamera.read(). size: %d, flags: %d\n", bufSize, flags);

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

                    //printf("[camera] RPiCamera.read() timestamp: %llu\n", ts);

                    if (flags & FLAG_ENDOFFRAME)
                        return true;
                }

                if (!aval)
                    usleep(1000);
            }
        }
        catch (const OMXExeption& ex)
        {
            printf("[camera] exception RPiCamera.read(): %s\n", ex.what());
        }
        catch (const char * msg)
        {
            printf("[camera] exception RPiCamera.read(): %s\n", msg);
        }

        //printf("[camera] RPiCamera.read() failed\n");
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
                        printf("Failed to write to output file: %s\n", strerror(errno));
                        break;
                    }
#endif
                    printf("[High] buffer -> out file: %d/%d flags: %d\n",
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
}
