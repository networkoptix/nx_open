// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "hw_video_api.h"

#include <CoreVideo/CoreVideo.h>

#include <QtMultimedia/QVideoFrame>
#include <QtMultimedia/private/qabstractvideobuffer_p.h>

#include <nx/utils/log/log.h>
#include <nx/media/video_frame.h>

#include "mac_texture_mapper.h"

extern "C" {
#include <libavutil/frame.h>
} // extern "C"

namespace nx::media {

namespace {

QVideoFrameFormat::PixelFormat toQtPixelFormat(OSType pixFormat)
{
    switch (pixFormat)
    {
        case kCVPixelFormatType_420YpCbCr8Planar:
        case kCVPixelFormatType_420YpCbCr8PlanarFullRange:
            return QVideoFrameFormat::Format_YUV420P;
        case kCVPixelFormatType_422YpCbCr8:
            return QVideoFrameFormat::Format_UYVY;
        case kCVPixelFormatType_32BGRA:
            return QVideoFrameFormat::Format_BGRA8888;
        case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
        case kCVPixelFormatType_420YpCbCr8BiPlanarFullRange:
            return QVideoFrameFormat::Format_NV12;
        default:
            return QVideoFrameFormat::Format_Invalid;
    }
}

class MacMemoryBuffer: public QAbstractVideoBuffer
{
    using base_type = QAbstractVideoBuffer;

public:
    MacMemoryBuffer(const AVFrame* frame):
        base_type(QVideoFrame::NoHandle),
        m_frame(frame)
    {
    }

    ~MacMemoryBuffer()
    {
    }

    virtual MapData map(QVideoFrame::MapMode mode) override
    {
        MapData data;

        NX_ASSERT(mode == QVideoFrame::ReadOnly);

        data.nPlanes = 1;

        CVPixelBufferRef pixbuf = (CVPixelBufferRef) m_frame->data[3];

        const auto ret = CVPixelBufferLockBaseAddress(pixbuf, kCVPixelBufferLock_ReadOnly);
        if (ret != kCVReturnSuccess)
            return {};

        if (CVPixelBufferIsPlanar(pixbuf))
        {
            data.nPlanes = CVPixelBufferGetPlaneCount(pixbuf);
            for (int i = 0; i < data.nPlanes; i++)
            {
                data.data[i] = (uint8_t*) CVPixelBufferGetBaseAddressOfPlane(pixbuf, i);
                data.bytesPerLine[i] = CVPixelBufferGetBytesPerRowOfPlane(pixbuf, i);
                data.size[i] = data.bytesPerLine[i] * CVPixelBufferGetHeightOfPlane(pixbuf, i);
            }
        }
        else
        {
            data.nPlanes = 1;
            data.data[0] = (uchar*) CVPixelBufferGetBaseAddress(pixbuf);
            data.bytesPerLine[0] = CVPixelBufferGetBytesPerRow(pixbuf);
            data.size[0] = data.bytesPerLine[0] * CVPixelBufferGetHeight(pixbuf);
        }

        return data;
    }

    virtual void unmap() override
    {
        CVPixelBufferRef pixbuf = (CVPixelBufferRef) m_frame->data[3];
        CVPixelBufferUnlockBaseAddress(pixbuf, kCVPixelBufferLock_ReadOnly);
    }

    virtual std::unique_ptr<QVideoFrameTextures> mapTextures(QRhi* rhi) override
    {
        CVPixelBufferRef buffer = (CVPixelBufferRef) m_frame->data[3];
        if (!m_textureSet)
            m_textureSet = MacTextureMapper::create(buffer, rhi);
        return m_textureSet->mapTextures(rhi);
    }

private:
    std::unique_ptr<MacTextureMapper> m_textureSet;
    const AVFrame* m_frame = nullptr;
};

} // namespace

class MacVideoApiEntry: public VideoApiRegistry::Entry
{
public:
    MacVideoApiEntry()
    {
        VideoApiRegistry::instance()->add(AV_HWDEVICE_TYPE_VIDEOTOOLBOX, this);
    }

    virtual AVHWDeviceType deviceType() const override
    {
        return AV_HWDEVICE_TYPE_VIDEOTOOLBOX;
    }

    virtual nx::media::VideoFramePtr makeFrame(const AVFrame* frame) const override
    {
        if (!frame)
            return {};

        if (!NX_ASSERT(frame->format == AV_PIX_FMT_VIDEOTOOLBOX))
            return {};

        const QSize frameSize(frame->width, frame->height);
        const int64_t timestampUs = frame->pkt_dts;

        CVPixelBufferRef pixBuf = (CVPixelBufferRef) frame->data[3];
        const auto qtPixelFormat = toQtPixelFormat(CVPixelBufferGetPixelFormatType(pixBuf));

        QAbstractVideoBuffer* buffer = new MacMemoryBuffer(frame);
        auto result = std::make_shared<VideoFrame>(
            buffer,
            QVideoFrameFormat{frameSize, qtPixelFormat});
        result->setStartTime(timestampUs);

        return result;
    }
};

MacVideoApiEntry g_macVideoApiEntry;

} // namespace nx::media
