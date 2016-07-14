#ifndef __FFMPEG_HELPER_H
#define __FFMPEG_HELPER_H

#ifdef ENABLE_DATA_PROVIDERS

#include "core/resource/resource_fwd.h"

extern "C"
{
    #include <libavformat/avformat.h>
}

#include <QtCore/QIODevice>


QString codecIDToString(CodecID codecID);
QString getAudioCodecDescription(AVCodecContext* codecContext);


class QnFfmpegHelper
{
private:
    enum CodecCtxField { Field_RC_EQ, Field_EXTRADATA, Field_INTRA_MATRIX, Field_INTER_MATRIX, Field_OVERRIDE, Field_Channels, Field_SampleRate, Field_Sample_Fmt, Field_BitsPerSample,
                         Field_CodedWidth, Field_CodedHeight};
    static void appendCtxField(QByteArray *dst, CodecCtxField field, const char* data, int size);
public:
    static void serializeCodecContext(const AVCodecContext *ctx, QByteArray *data);
    static AVCodecContext *deserializeCodecContext(const char *data, int dataLen);

    static AVIOContext* createFfmpegIOContext(QnStorageResourcePtr resource, const QString& url, QIODevice::OpenMode openMode, int ioBlockSize = 32768);
    static AVIOContext* createFfmpegIOContext(QIODevice* ioDevice, int ioBlockSize = 32768);
    static void closeFfmpegIOContext(AVIOContext* ioContext);
    static qint64 getFileSizeByIOContext(AVIOContext* ioContext);
    static void deleteCodecContext(AVCodecContext* ctx);
};

#endif // ENABLE_DATA_PROVIDERS

#endif // __FFMPEG_HELPER_H
