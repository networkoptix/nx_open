#ifndef __FFMPEG_HELPER_H
#define __FFMPEG_HELPER_H

QString codecIDToString(CodecID codecID);

class VC1SequenceHeader;

class FrameTypeExtractor 
{
public:
    FrameTypeExtractor(AVCodecContext* context);
    ~FrameTypeExtractor();

    enum FrameType {UnknownFrameType, I_Frame, P_Frame, B_Frame};
    FrameType getFrameType(const quint8* data, int dataLen);

    AVCodecContext* getContext() const { return m_context; }
    CodecID getCodec() const { return m_context->codec_id; }
private:

    FrameType getH264FrameType(const quint8* data, int size);
    FrameType getMpegVideoFrameType(const quint8* data, int size);
    FrameType getWMVFrameType(const quint8* data, int size);
    FrameType getVCFrameType(const quint8* data, int size);
    FrameType getMpeg4FrameType(const quint8* data, int size);

    void decodeWMVSequence(const quint8* data, int size);
private:
    AVCodecContext* m_context;
    CodecID m_codecId;
    VC1SequenceHeader* m_vcSequence;
    bool m_dataWithNalPrefixes;
};

class QnFfmpegHelper
{
private:
    enum CodecCtxField { Field_RC_EQ, Field_EXTRADATA, Field_INTRA_MATRIX, Field_INTER_MATRIX, Field_OVERRIDE, Field_Channels, Field_SampleRate, Field_Sample_Fmt };
    static void appendCtxField(QByteArray *dst, CodecCtxField field, const char* data, int size);
public:
    static void serializeCodecContext(const AVCodecContext *ctx, QByteArray *data);
    static AVCodecContext *deserializeCodecContext(const char *data, int dataLen);
};

#endif // __FFMPEG_HELPER_H
