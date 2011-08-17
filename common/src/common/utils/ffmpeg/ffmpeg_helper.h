#ifndef FFMPEG_HELPER_H
#define FFMPEG_HELPER_H

class QByteArray;
class QMutex;

struct AVCodecContext;

// where is not marshaling at current version. So, need refactor for using with non x86 CPU in future

class QnFfmpegHelper
{
public:
    static QMutex *global_ffmpeg_mutex();

    static void serializeCodecContext(const AVCodecContext *ctx, QByteArray *data);
    static AVCodecContext *deserializeCodecContext(const char *data, int dataLen);
};

#endif // FFMPEG_HELPER_H
