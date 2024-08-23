// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef _RPI_CAMERA_H_
#define _RPI_CAMERA_H_

#include <memory>
#include <mutex>

#include "rpi_omx.h"
#include "rw_lock.h"

/// Raspberry Pi camera unit and configuration
namespace rpi_cam
{
    /// @sa OMX_IMAGEFILTERTYPE
    typedef enum
    {
        Filter_None,        ///< No filter
        Filter_Noise,       ///< Remove noise
        Filter_Emboss,      ///< Emboss
        Filter_Negative,    ///< Negative
        Filter_Sketch,      ///< Sketch
        Filter_OilPaint,    ///< OilPaint
        Filter_Hatch,       ///< Hatch
        Filter_Gpen,        ///< Gpen
        Filter_Antialias,   ///< Antialias
        Filter_DeRing,      ///< DeRing
        Filter_Solarize     ///< Solarize
    } ImageFilter;

    /// Resolutions and common camera parameters
    struct CameraParameters
    {
        CameraParameters(rpi_omx::VideoFromat fmt = rpi_omx::VF_1280x960, ImageFilter filt = Filter_None)
        :   vfmt(fmt),
            filter(filt),
            sharpness(0),
            contrast(0),
            saturation(0),
            brightness(50)
        {}

        rpi_omx::VideoFromat vfmt;
        unsigned filter;            ///< Image filter @sa rpi_cam::ImageFilter
        int sharpness;              ///< -100..100
        int contrast;               ///< -100..100
        int saturation;             ///< -100..100
        int brightness;             ///< 0..100
    };

    /// Encoder parameters
    struct EncoderParameters
    {
        EncoderParameters()
        :   vfmt(rpi_omx::VF_1280x960),
            bitrateKbps(0)
        {}

        rpi_omx::VideoFromat vfmt;
        unsigned bitrateKbps;
    };

    /// Raspberry Pi camera unit
    class RPiCamera
    {
    public:
        typedef rw_lock::OneWriterManyReaders RWLock;

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
        static constexpr unsigned IFRAME_PERIOD() { return 5; }

        static void init();
        static void deinit();
        static const rpi_omx::VideoFromat * getVideoFormats(unsigned& num, bool resized = false);

        RPiCamera(const CameraParameters& camParams);
        ~RPiCamera();

        bool isOK() const;
        bool read(unsigned streamNo, std::vector<uint8_t>& data, uint64_t& timeStamp, unsigned& flags);
        void getEncoderConfig(unsigned streamNo, unsigned& width, unsigned& height, unsigned& fps, unsigned& bitrateKbps) const;
        void configEncoder(unsigned streamNo, unsigned width, unsigned height, unsigned framerate, unsigned bitrateKbps);
        void update(); // update if need it

    private:
        static std::recursive_mutex m_mutex; // locks entire class, not just an object
        std::unique_ptr<rpi_omx::Camera> m_camera;
        std::unique_ptr<rpi_omx::VideoSplitter> m_vsplitter;
        std::unique_ptr<rpi_omx::Resizer> m_resizer;
        std::unique_ptr<rpi_omx::Encoder> m_encoders[2];
        CameraParameters m_camParams;
        EncoderParameters m_encParams[2];
        RWLock m_rw;
        bool m_needUpdate;

        static void updateRatio(rpi_omx::VideoFromat& fmt);
        static const rpi_omx::VideoFromat& getResizedFormat(const rpi_omx::VideoFromat& f);

        //

        void prepare(const CameraParameters& camParams, const EncoderParameters& encParams0, const EncoderParameters& encParams1);
        void release();
        void resetErrors();

        void switchState(OMX_STATETYPE state);

        void configCamera(const CameraParameters& params);
        void configEncoders(const EncoderParameters& params0, const EncoderParameters& params1);

        void setupTunnels();
        void teardownTunnels();

        void enablePorts();
        void disablePorts();

        void allocateBuffers();
        void returnBuffers();
        void flushBuffers();
        void freeBuffers();

        void startCapture();
        void stopCapture();

        void dumpPorts();
    };
}

#endif
