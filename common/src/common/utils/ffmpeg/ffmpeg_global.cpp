#include "ffmpeg_global.h"

#include <QtCore/QFile>
#include <QtCore/QMutex>

extern "C" {
#include <libavutil/avstring.h>
}

#include "ffmpeg_codec.h"
#include "utils/common/log.h"

Q_GLOBAL_STATIC(QMutex, global_ffmpeg_mutex)

QMutex *QnFFmpeg::globalMutex()
{
    QMutex *mutex = global_ffmpeg_mutex();
    if (!mutex)
    {
        qWarning("QnFFmpeg::globalMutex() was called after the application exit!");
        cl_log.log(QLatin1String("QnFFmpeg::globalMutex() was called after the application exit!"), cl_logALWAYS);
        mutex = new QMutex; // it's better to leak than to crash, no?
    }

    return mutex;
}

#include "ufile_protocol.cpp"

static bool ffmpeg_initialized = false;

void QnFFmpeg::initialize()
{
    if (ffmpeg_initialized)
        return;

    QMutexLocker locker(QnFFmpeg::globalMutex());
    // after unlocking, check again
    if (!ffmpeg_initialized)
    {
        // must be called before using avcodec
        av_register_all();

        // register all the codecs (you can also register only the codec you wish to have smaller code
        avcodec_register_all();

        avcodec_init();

        // register URLPrococol to allow ffmpeg use files with non-latin filenames
        av_register_protocol2(&ufile_protocol, sizeof(ufile_protocol));

        cl_log.log(QLatin1String("FFMEG version = "), (int)avcodec_version(), cl_logALWAYS);

        ffmpeg_initialized = true;
    }
}

static inline void ensureInitialized()
{
    if (!ffmpeg_initialized)
        QnFFmpeg::initialize();
}

AVCodec *QnFFmpeg::findDecoder(CodecID codecId)
{
    ensureInitialized();

    AVCodec *codec = 0;

    //codecId = internalCodecIdToFfmpeg(codecId);
    if (codecId != CODEC_ID_NONE)
    {
        QMutexLocker locker(QnFFmpeg::globalMutex());
        codec = avcodec_find_decoder(codecId);
    }

    return codec;
}

AVCodec *QnFFmpeg::findEncoder(CodecID codecId)
{
    ensureInitialized();

    AVCodec *codec = 0;

    //codecId = internalCodecIdToFfmpeg(codecId);
    if (codecId != CODEC_ID_NONE)
    {
        QMutexLocker locker(QnFFmpeg::globalMutex());
        codec = avcodec_find_encoder(codecId);
    }

    return codec;
}

bool QnFFmpeg::openCodec(AVCodecContext *context, AVCodec *codec)
{
    ensureInitialized();

    QMutexLocker locker(QnFFmpeg::globalMutex());
    return avcodec_open(context, codec) == 0;
}

void QnFFmpeg::closeCodec(AVCodecContext *context)
{
    ensureInitialized();

    QMutexLocker locker(QnFFmpeg::globalMutex());
    avcodec_close(context);
}

AVFormatContext *QnFFmpeg::openFileContext(const QString &filePath)
{
    ensureInitialized();

    AVFormatContext *fileContext = 0;

    const QByteArray url = "ufile:" + QFile::encodeName(filePath);
    int err = av_open_input_file(&fileContext, url.constData(), NULL, 0, NULL);
    if (err == 0)
    {
        QMutexLocker locker(QnFFmpeg::globalMutex());
        if (av_find_stream_info(fileContext) >= 0)
            return fileContext;
    }

    QnFFmpeg::closeFileContext(fileContext);

    return 0;
}

void QnFFmpeg::closeFileContext(AVFormatContext *fileContext)
{
    ensureInitialized();

    if (fileContext)
        av_close_input_file(fileContext);
}
