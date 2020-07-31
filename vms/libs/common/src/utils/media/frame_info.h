#pragma once

#ifdef ENABLE_DATA_PROVIDERS

#include <atomic>
#include <memory>

#include <QtCore/QAtomicInt>
#include <QtGui/QImage>
#include <QtMultimedia/QVideoFrame>
#include <QtGui/QOpenGLFunctions>

extern "C" {

#include <libavcodec/avcodec.h>

} // extern "C"

#include <nx/streaming/media_data_packet.h>
#include <utils/media/ffmpeg_helper.h>

#include "transcoding/filters/filter_helper.h"

#define AV_REVERSE_BLOCK_START QnAbstractMediaData::MediaFlags_ReverseBlockStart
#define AV_REVERSE_REORDERED   QnAbstractMediaData::MediaFlags_ReverseReordered

enum class MemoryType
{
    SystemMemory,
    VideoMemory,
};

namespace nx::media {

class QuickSyncVideoDecoderImpl;

};

class AbstractVideoSurface
{
public:
    virtual ~AbstractVideoSurface() {}
    virtual bool renderToRgb(bool isNewTexture, GLuint textureId, QOpenGLContext* context) = 0;
    virtual AVFrame lockFrame() = 0;
    virtual void unlockFrame() = 0;
};

/**
 * Decoded frame, ready to be rendered.
 *
 * ATTENTION: AVFrame's field pkt_dts is filled with the time since epoch in microseconds, instead
 * of time_base units as ffmpeg states in the documentation.
 */
class CLVideoDecoderOutput: public AVFrame
{
public:
    //!Stores picture data. If NULL, picture data is stored in \a AVFrame fields

    CLVideoDecoderOutput();
    CLVideoDecoderOutput(int targetWidth, int targetHeight, int targetFormat);
    CLVideoDecoderOutput(QImage image);
    ~CLVideoDecoderOutput();

    MemoryType memoryType() const { return m_memoryType; }
    bool isEmpty();
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
    static bool imagesAreEqual(const CLVideoDecoderOutput* img1, const CLVideoDecoderOutput* img2, unsigned int max_diff);
    static bool isPixelFormatSupported(AVPixelFormat format);

    void saveToFile(const char* filename);
    void clean();
    void setUseExternalData(bool value);
    bool isExternalData() const { return m_useExternalData; }
    void reallocate(int newWidth, int newHeight, int format);
    void reallocate(const QSize& size, int format);
    void reallocate(int newWidth, int newHeight, int newFormat, int lineSizeHint);
    void memZero();

    CLVideoDecoderOutput* rotated(int angle) const;
    /** Scale frame to new size */
    CLVideoDecoderOutput* scaled(const QSize& newSize, AVPixelFormat newFormat = AV_PIX_FMT_NONE) const;

    QSize size() const { return QSize(width, height); }

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
    FrameMetadata metadata; // addition data associated with video frame

private:
    bool m_useExternalData = false; // pointers only copied to this frame
    std::unique_ptr<AbstractVideoSurface> m_surface;
    MemoryType m_memoryType = MemoryType::SystemMemory;

private:
    bool invalidScaleParameters(const QSize& size) const;
    static void copyPlane(unsigned char* dst, const unsigned char* src, int width, int dst_stride, int src_stride, int height);
    static bool equalPlanes(const unsigned char* plane1, const unsigned char* plane2, int width, int stride1, int stride2, int height, int max_diff);

    CLVideoDecoderOutput( const CLVideoDecoderOutput& );
    const CLVideoDecoderOutput& operator=( const CLVideoDecoderOutput& );

    bool convertUsingSimdIntrTo(const AVFrame* avFrame) const;
};
typedef QSharedPointer<CLVideoDecoderOutput> CLVideoDecoderOutputPtr;
typedef QSharedPointer<const CLVideoDecoderOutput> CLConstVideoDecoderOutputPtr;

class ScreenshotInterface
{
public:
    virtual CLVideoDecoderOutputPtr getScreenshot(bool anyQuality) = 0;
    virtual QImage getGrayscaleScreenshot() = 0;
};


#endif // ENABLE_DATA_PROVIDERS
