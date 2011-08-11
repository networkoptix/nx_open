#ifndef __FFMPEG_HELPER_H
#define __FFMPEG_HELPER_H

#include <QByteArray>
class AVCodecContext;

class QnFfmpegHelper
{
public:
    static void serializeCodecContext(const AVCodecContext* ctx, QByteArray* data);
    static AVCodecContext* deserializeCodecContext(const char* data, int dataLen);
};

#endif
