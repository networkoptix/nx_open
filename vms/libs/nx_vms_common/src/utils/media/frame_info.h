// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>
#include <memory>

#include <QtCore/QSharedPointer>
#include <QtCore/QSize>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>

} // extern "C"

#include <nx/media/ffmpeg_helper.h>
#include <nx/media/media_data_packet.h>

enum class MemoryType
{
    SystemMemory,
    VideoMemory,
};

enum class SurfaceType
{
    Intel,
    Nvidia,
};

class AbstractVideoSurface
{
public:
    virtual ~AbstractVideoSurface() {}
    virtual AVFrame lockFrame() = 0;
    virtual void unlockFrame() = 0;
    virtual SurfaceType type() = 0;
};

class CLVideoDecoderOutput;
using CLVideoDecoderOutputPtr = QSharedPointer<CLVideoDecoderOutput>;
using CLConstVideoDecoderOutputPtr = QSharedPointer<const CLVideoDecoderOutput>;

/**
 * Decoded frame, ready to be rendered.
 *
 * ATTENTION: AVFrame's field pkt_dts is filled with the time since epoch in microseconds, instead
 * of time_base units as ffmpeg states in the documentation.
 */
class NX_VMS_COMMON_API CLVideoDecoderOutput: public AVFrame
{
public:
    //!Stores picture data. If NULL, picture data is stored in \a AVFrame fields

    CLVideoDecoderOutput();
    CLVideoDecoderOutput(int targetWidth, int targetHeight, int targetFormat);
    CLVideoDecoderOutput(const QImage& image);
    ~CLVideoDecoderOutput();

    MemoryType memoryType() const { return m_memoryType; }
    bool isEmpty() const;
    void attachVideoSurface(std::unique_ptr<AbstractVideoSurface> surface);
    AbstractVideoSurface* getVideoSurface() const { return m_surface.get(); }

    QImage toImage() const;

    /**
     * Copies the frame to another already allocated frame, converting to the required pixel format
     * if needed.
     */
    bool convertTo(const AVFrame* avFrame) const;

    void copyFrom(const CLVideoDecoderOutput* src);
    void copyDataOnlyFrom(const AVFrame* src);
    static bool isPixelFormatSupported(AVPixelFormat format);

    void saveToFile(const char* filename); //< Is used for debugging.
    void clean();
    void setUseExternalData(bool value);
    bool isExternalData() const { return m_useExternalData; }
    bool reallocate(int newWidth, int newHeight, int format);
    bool reallocate(const QSize& size, int format);
    bool reallocate(int newWidth, int newHeight, int newFormat, int lineSizeHint);
    void memZero();

    /** Scale frame to new size */
    CLVideoDecoderOutputPtr scaled(
        const QSize& newSize,
        AVPixelFormat newFormat = AV_PIX_FMT_NONE,
        bool keepAspectRatio = false,
        int scaleFlags = SWS_BICUBIC) const;

    QSize size() const { return QSize(width, height); }
    int64_t sizeBytes() const;

    /** Assign misc fields except but no video data */
    void assignMiscData(const CLVideoDecoderOutput* other);
    void fillRightEdge();

    static AVPixelFormat fixDeprecatedPixelFormat(AVPixelFormat original);

    /**
     * Return raw data of the image data.
     * @return All planes without padding.
     */
    QByteArray rawData() const;

public:
    //QSharedPointer<QnAbstractPictureDataRef> picData;

    QnAbstractMediaData::MediaFlags flags = QnAbstractMediaData::MediaFlags_None;

    /** Pixel width to pixel height ratio. Some videos have non-square pixels, we support that. */
    double sample_aspect_ratio = 0.0;

    /** Number of the video channel in video layout. */
    int channel = 0;

    /* Needed to downscale video memory surface on rendering stage.
     * Using scale factor instead of output texture size help to reduce VPP reinitialization
     */
    int scaleFactor = 1;

private:
    bool m_useExternalData = false; // pointers only copied to this frame
    std::unique_ptr<AbstractVideoSurface> m_surface;
    MemoryType m_memoryType = MemoryType::SystemMemory;

private:
    bool invalidScaleParameters(const QSize& size) const;
    static void copyPlane(
        uint8_t* dst, const uint8_t* src, int width, int dstStride, int srcStride, int height);

    CLVideoDecoderOutput( const CLVideoDecoderOutput& );
    const CLVideoDecoderOutput& operator=( const CLVideoDecoderOutput& );

    bool convertUsingSimdIntrTo(const AVFrame* avFrame) const;
};

class ScreenshotInterface
{
public:
    virtual CLVideoDecoderOutputPtr getScreenshot(bool anyQuality) = 0;
    virtual QImage getGrayscaleScreenshot() = 0;
};
