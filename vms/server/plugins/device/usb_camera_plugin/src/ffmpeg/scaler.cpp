#include "scaler.h"

extern "C" {
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
} // extern "C"

namespace nx::usb_cam::ffmpeg {

Scaler::~Scaler()
{
    uninitialize();
}

void Scaler::configure(nxcip::Resolution targetResolution, AVPixelFormat targetPixelFormat)
{
    m_targetParams.width = targetResolution.width;
    m_targetParams.height = targetResolution.height;
    m_targetParams.pixelFormat = targetPixelFormat;
    uninitialize(); // will initialized on ensureInitialized()
}

int Scaler::scale(const Frame* inputFrame, Frame** result)
{
    int status = ensureInitialized(inputFrame);
    if (status < 0)
        return status;

    status = sws_scale(
        m_scaleContext,
        inputFrame->frame()->data,
        inputFrame->frame()->linesize,
        0,
        inputFrame->frame()->height,
        m_scaledFrame->frame()->data,
        m_scaledFrame->frame()->linesize);

    if (status < 0)
        return status;

    m_scaledFrame->frame()->pts = inputFrame->frame()->pts;
    m_scaledFrame->frame()->pkt_pts = inputFrame->frame()->pkt_pts;
    m_scaledFrame->frame()->pkt_dts = inputFrame->frame()->pkt_dts;
    *result = m_scaledFrame.get();
    return status;
}

int Scaler::ensureInitialized(const Frame* inputFrame)
{
    PictureParameters currentSourceParams {
        inputFrame->frame()->width,
        inputFrame->frame()->height,
        (AVPixelFormat)inputFrame->frame()->format
    };

    if (m_scaleContext && m_sourceParams == currentSourceParams)
        return 0;

    uninitialize();
    m_sourceParams = currentSourceParams;
    m_scaleContext = sws_getContext(
        m_sourceParams.width,
        m_sourceParams.height,
        ffmpeg::utils::unDeprecatePixelFormat(m_sourceParams.pixelFormat),
        m_targetParams.width,
        m_targetParams.height,
        ffmpeg::utils::unDeprecatePixelFormat(m_targetParams.pixelFormat),
        SWS_FAST_BILINEAR,
        nullptr,
        nullptr,
        nullptr);

    if (!m_scaleContext)
        return AVERROR(ENOMEM);

    return initializeScaledFrame(m_targetParams);
}

int Scaler::initializeScaledFrame(const PictureParameters & pictureParams)
{
    auto scaledFrame = std::make_unique<ffmpeg::Frame>();
    if (!scaledFrame || !scaledFrame->frame())
        return AVERROR(ENOMEM);

    int status = scaledFrame->getBuffer(
        pictureParams.pixelFormat,
        pictureParams.width,
        pictureParams.height);

    if (status < 0)
        return status;

    m_scaledFrame = std::move(scaledFrame);
    return 0;
}

void Scaler::uninitialize()
{
    if (m_scaleContext)
    {
        sws_freeContext(m_scaleContext);
        m_scaleContext = nullptr;
    }
    m_scaledFrame.reset(nullptr);
}

} // nx::usb_cam::ffmpeg
