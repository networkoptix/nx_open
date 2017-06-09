#pragma once

#include "core/resource/resource_fwd.h"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avio.h>
}

#include <QtCore/QIODevice>

#include "audioformat.h"
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

private:
    static void copyMediaContextFieldsToAvCodecContext(
        AVCodecContext* av, const QnConstMediaContextPtr& media);

    /** Singleton.
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
    QnFfmpegAudioHelper(AVCodecContext* decoderContex);
    ~QnFfmpegAudioHelper();

    void copyAudioSamples(quint8* dst, const AVFrame* src);
private:
    SwrContext* m_swr;
};

struct QnFfmpegAvPacket: AVPacket
{
    QnFfmpegAvPacket(uint8_t* data = nullptr, int size = 0);
    ~QnFfmpegAvPacket();
};
