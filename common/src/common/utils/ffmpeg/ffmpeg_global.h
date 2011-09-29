#ifndef FFMPEG_GLOBAL_H
#define FFMPEG_GLOBAL_H

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

class QMutex;

#define DEFAULT_AUDIO_FRAME_SIZE (AVCODEC_MAX_AUDIO_FRAME_SIZE * 2)
#define MAX_AUDIO_FRAME_SIZE (DEFAULT_AUDIO_FRAME_SIZE * 5)

/**
 * Provides a thread-safe versions of commmon libav functions.
 *
 * The functions that need to be protected when working with libavcodec:
 * avcodec_register_all(), avdevice_register_all(), av_register_all();
 * avcodec_find_encoder(), avcodec_find_decoder();
 * avcodec_open(), avcodec_close();
 * av_find_stream_info().
 *
 * In case you need to use these directly, make sure you protect the call with
 * the mutex provided by QnFFmpeg::globalMutex().
 */

struct QnFFmpeg
{
    static QMutex *globalMutex();

    static void initialize();

    static AVCodec *findDecoder(CodecID codecId);
    static AVCodec *findEncoder(CodecID codecId);

    static bool openCodec(AVCodecContext *context, AVCodec *codec);
    static void closeCodec(AVCodecContext *context);

    static AVFormatContext *openFileContext(const QString &filePath);
    static void closeFileContext(AVFormatContext *fileContext);
};

#endif // FFMPEG_GLOBAL_H
