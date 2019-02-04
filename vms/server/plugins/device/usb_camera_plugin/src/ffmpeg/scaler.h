#pragma once

#include "frame.h"
#include "camera/codec_parameters.h"

struct SwsContext;

namespace nx::usb_cam::ffmpeg {

class Scaler
{
public:
    ~Scaler();
    void configure(nxcip::Resolution targetResolution, AVPixelFormat targetPixelFormat);
    void uninitialize();
    int scale(const Frame* source, Frame** result);

private:
    struct PictureParameters
    {
        int width;
        int height;
        AVPixelFormat pixelFormat;

        bool operator==(const PictureParameters& rhs) const
        {
            return width == rhs.width && height == rhs.height && pixelFormat == rhs.pixelFormat;
        }

        bool operator!=(const PictureParameters& rhs) const
        {
            return !operator==(rhs);
        }
    };

private:
    int initializeScaledFrame(const PictureParameters& pictureParams);
    int ensureInitialized(const Frame* inputFrame);

private:
    std::unique_ptr<ffmpeg::Frame> m_scaledFrame;
    PictureParameters m_sourceParams;
    PictureParameters m_targetParams;
    SwsContext* m_scaleContext = nullptr;
};

}
