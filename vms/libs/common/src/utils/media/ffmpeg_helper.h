#pragma once

#include "core/resource/resource_fwd.h"

extern "C" {

#include <libavcodec/avcodec.h>
#include <libavformat/avio.h>
#include <libavutil/mem.h>
#include <libavutil/imgutils.h>

} // extern "C"

#include <QtCore/QIODevice>

#include "audioformat.h"
#include <nx/streaming/config.h>
#include <nx/streaming/media_context.h>
#include <nx/streaming/media_context_serializable_data.h>

/** Static.
 * Contains utilities which rely on ffmpeg implementation.
 */
class QnFfmpegHelper
{
public:
    /**
     * Copy new value (array of bytes) to a field of an ffmpeg structure (e.g. AVCodecContext),
     * deallocating if not null, then allocating via av_malloc().
     * If size is 0, data can be null.
     */
    static void copyAvCodecContextField(void **fieldPtr, const void* data, size_t size);

    /**
     * Either the existing AVCodecContext (for QnAvCodecMediaContext), or the one constructed from fields
     * (for QnPlainDataMediaContext) is copied to the provided AVCodecContext.
     * @param av Should be already allocated but not filled yet.
     */
    static void mediaContextToAvCodecContext(AVCodecContext* av, const QnConstMediaContextPtr& media);

    /**
     * @return Either a codec found in ffmpeg registry, or a static instance of a stub AVCodec in case
     * the proper codec is not available in ffmpeg; never null.
     */
    static AVCodec* findAvCodec(AVCodecID codecId);

    /**
     * @return Newly allocated AVCodecContext with a proper codec, codec_id and coded_type; never null.
     */
    static AVCodecContext* createAvCodecContext(AVCodecID codecId);

    /**
     * @return Newly allocated AVCodecContext with data deep-copied via avcodec_copy_context(); never null.
     * @param context Not null.
     */
    static AVCodecContext* createAvCodecContext(const AVCodecContext* context);

    /**
    * Copy the settings of the source AVCodecContext into the destination AVCodecContext.
    * The resulting destination codec context will be unopened.
    * @return AVERROR() on error (e.g. memory allocation error), 0 on success
    */
    static int copyAvCodecContex(AVCodecContext* dst, const AVCodecContext* src);

    /**
     * Close and deep-deallocate the context.
     * @param context If null, do nothing.
     */
    static void deleteAvCodecContext(AVCodecContext* context);

    /**
     * Deserialize MediaContext from binary format used in v2.5. Requires ffmpeg impl to fill
     * default values. QnMediaContextSerializableData fields are expected to be non-initialized.
     */
    static bool deserializeMediaContextFromDeprecatedFormat(
        QnMediaContextSerializableData* context, const char* data, int dataLen);

    static AVIOContext* createFfmpegIOContext(QnStorageResourcePtr resource, const QString& url, QIODevice::OpenMode openMode, int ioBlockSize = 32768);
    static AVIOContext* createFfmpegIOContext(QIODevice* ioDevice, int ioBlockSize = 32768);
    static void closeFfmpegIOContext(AVIOContext* ioContext);
    static qint64 getFileSizeByIOContext(AVIOContext* ioContext);

    static QString getErrorStr(int errnum);

    static int audioSampleSize(AVCodecContext* ctx);

    static AVSampleFormat fromQtAudioFormatToFfmpegSampleType(const QnAudioFormat& format);
    static AVCodecID fromQtAudioFormatToFfmpegPcmCodec(const QnAudioFormat& format);

    static int getDefaultFrameSize(AVCodecContext* context);

private:
    static void copyMediaContextFieldsToAvCodecContext(
        AVCodecContext* av, const QnConstMediaContextPtr& media);

    /**
     * Holds a globally shared static instance of a stub AVCodec used for
     * AVCodecContext when the proper codec is not available in ffmpeg.
     */
    class StaticHolder
    {
    public:
        // Instance is not const because ffmpeg requires non-const AVCodec.
        static StaticHolder instance;

        AVCodec avCodec;

    private:
        StaticHolder()
        {
            memset(&avCodec, 0, sizeof(avCodec));
            avCodec.id = AV_CODEC_ID_NONE;
            avCodec.type = AVMEDIA_TYPE_VIDEO;
        }

        StaticHolder(const StaticHolder&) /*= delete*/;
        void operator=(const StaticHolder&) /*= delete*/;
    };
};

struct SwrContext;

class QnFfmpegAudioHelper
{
public:
    QnFfmpegAudioHelper(AVCodecContext* decoderContext);
    ~QnFfmpegAudioHelper();

    void copyAudioSamples(quint8* dst, const AVFrame* src) const;

private:
    SwrContext* m_swr;
};

struct QnFfmpegAvPacket: AVPacket
{
    QnFfmpegAvPacket(uint8_t* data = nullptr, int size = 0);
    ~QnFfmpegAvPacket();

    QnFfmpegAvPacket(const QnFfmpegAvPacket&) = delete;
    QnFfmpegAvPacket& operator=(const QnFfmpegAvPacket&) = delete;
};

/**
 * Wrapper around ffmpeg's AVFrame which either owns an AVFrame properly de-allocating it, or wraps
 * around a given pointer to an existing AVFrame without owning it (doing nothing in the
 * destructor).
 */
class AvFrameHolder
{
public:
    /**
     * Creates and owns an instance of AVFrame, allocating its buffers and setting its fields. On
     * error, fails an assertion and holds a null pointer to AVFrame.
     */
    AvFrameHolder(int width, int height, AVPixelFormat pixelFormat):
        m_avFrame(av_frame_alloc()),
        m_owned(true)
    {
        if (!NX_ASSERT(m_avFrame) || !NX_ASSERT(width > 0, width) || !NX_ASSERT(height > 0, height)
            || !NX_ASSERT((int) pixelFormat >= 0, toString(pixelFormat)))
        {
            deallocateIfNeeded();
            return;
        }

        m_avFrame->width = width;
        m_avFrame->height = height;
        m_avFrame->format = pixelFormat;

        const int r = av_image_alloc(
            m_avFrame->data,
		    m_avFrame->linesize,
		    width,
		    height,
		    pixelFormat,
		    CL_MEDIA_ALIGNMENT);

        if (!NX_ASSERT(r > 0, lm("av_image_alloc() failed for pixel format %1").arg(pixelFormat)))
            deallocateIfNeeded();
    }

    /** Wraps the existing AVFrame without owning it. */
    AvFrameHolder(const AVFrame* avFrame):
        m_avFrame(const_cast<AVFrame*>(avFrame)),
        m_owned(false)
    {
        NX_CRITICAL(avFrame);
    }

    ~AvFrameHolder() { deallocateIfNeeded(); }

	AvFrameHolder(const AvFrameHolder&) = delete;
	AvFrameHolder& operator=(const AvFrameHolder&) = delete;
	AvFrameHolder(AvFrameHolder&&) = delete;
	AvFrameHolder& operator=(AvFrameHolder&&) = delete;

    const AVFrame* avFrame() const { return m_avFrame; }

private:
    void deallocateIfNeeded()
    {
        if (m_owned && m_avFrame)
        {
            av_freep(&m_avFrame->data[0]);
            av_frame_free(&m_avFrame);
        }
    }

private:
    AVFrame* m_avFrame;
    const bool m_owned;
};

QString toString(AVPixelFormat pixelFormat);
