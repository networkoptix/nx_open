#ifndef _RPI_CAMERA_H_
#define _RPI_CAMERA_H_

#include <vector>

#include "rpi_omx.h"

namespace rpi_cam
{
    ///
    typedef enum
    {
        Resolutions_default,
        Resolutions_1280x960_640x480,
        Resolutions_1280x960_320x240,
        Resolutions_1920x1080_640x360,
        Resolutions_1920x1080_480x270,
        Resolutions_1920x1080_320x180
    } ResolutionPair;

    ///
    typedef enum
    {
        Filter_None,
        Filter_Noise,
        Filter_Emboss,
        Filter_Negative,
        Filter_Sketch,
        Filter_OilPaint,
        Filter_Hatch,
        Filter_Gpen,
        Filter_Antialias,
        Filter_DeRing,
        Filter_Solarize
    } ImageFilter;

    struct CameraParameters
    {
        CameraParameters()
        :   resolition(Resolutions_1280x960_320x240),
            filter(Filter_None),
            sharpness(0),
            contrast(0),
            saturation(0),
            brightness(50)
        {}

        unsigned resolition;
        unsigned filter;
        int sharpness;
        int contrast;
        int saturation;
        int brightness;
    };

    ///
    class RPiCamera
    {
    public:
        enum
        {
            FLAG_EOS = OMX_BUFFERFLAG_EOS,
            FLAG_DECODEONLY = OMX_BUFFERFLAG_DECODEONLY,
            FLAG_DATACORRUPT = OMX_BUFFERFLAG_DATACORRUPT,
            FLAG_ENDOFFRAME = OMX_BUFFERFLAG_ENDOFFRAME,
            FLAG_SYNCFRAME = OMX_BUFFERFLAG_SYNCFRAME,
            FLAG_EXTRADATA = OMX_BUFFERFLAG_EXTRADATA,
            FLAG_CODECCONFIG = OMX_BUFFERFLAG_CODECCONFIG
        };

        static constexpr unsigned STREAMS_NUM() { return 2; }
        static constexpr unsigned BITRATE_0() { return 6000; }
        static constexpr unsigned BITRATE_1() { return 500; }
        static constexpr unsigned IFRAME_PERIOD() { return 6; }

        static constexpr const char * UID() { return "Raspberry Pi Camera 0"; }

        static void init();
        static void deinit();

        RPiCamera(const CameraParameters& params)
        :   m_fps(0),
            m_isOK(false)
        {
            m_bitrates[0] = BITRATE_0();
            m_bitrates[1] = BITRATE_1();

            config(params.resolition);
            m_isOK = prepare(params);
        }

        ~RPiCamera()
        {
            release();
        }

        bool isOK() const { return m_isOK; }

        void fillBuffer(unsigned streamNo);

        bool read(unsigned streamNo, std::vector<uint8_t>& data, uint64_t& timeStamp, unsigned& flags)
        {
            if (streamNo < STREAMS_NUM())
                return read(m_encoders[streamNo], data, timeStamp, flags);
            return false;
        }

        bool config(unsigned mode, unsigned fps = 0, unsigned bitrate0 = 0, unsigned bitrate1 = 0);
        void getConfig(unsigned streamNo, unsigned& width, unsigned& height, unsigned& fps, unsigned& bitrate) const;

        void reload()
        {
            // TODO
        }

        //void test();

    private:
        rpi_omx::Camera m_camera;
        rpi_omx::VideoSplitter m_vsplitter;
        rpi_omx::Resizer m_resizer;
        rpi_omx::Encoder m_encoders[2];
        rpi_omx::VideoFromat m_vfs[2];
        unsigned m_bitrates[2];
        unsigned m_fps;
        bool m_isOK;

        bool prepare(const CameraParameters& params);
        bool release();

        void switchState(OMX_STATETYPE state);

        void configCamera(const CameraParameters& params);
        void configEncoders();

        void tunnelPorts();
        void enablePorts();
        void disablePorts();

        void allocateBuffers();
        void returnBuffers();
        void flushBuffers();
        void freeBuffers();

        void startCapture() { m_camera.capture(rpi_omx::Camera::OPORT_VIDEO, OMX_TRUE); }
        void stopCapture() { m_camera.capture(rpi_omx::Camera::OPORT_VIDEO, OMX_FALSE); }

        void dumpPorts();

        bool read(rpi_omx::Encoder& encoder, std::vector<uint8_t>& data, uint64_t& timeStamp, unsigned& flags);
    };
}

#endif
