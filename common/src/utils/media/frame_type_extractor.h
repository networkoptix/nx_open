#ifndef FRAME_TYPE_EXTRACTOR_H
#define FRAME_TYPE_EXTRACTOR_H

class VC1SequenceHeader;

extern "C"
{
// For enum AVCodecID, struct AVCodecContext.
#include <libavcodec/avcodec.h>
}

#include <nx/streaming/media_context.h>

class FrameTypeExtractor
{
public:
    FrameTypeExtractor(const QnConstMediaContextPtr& context);
    FrameTypeExtractor(AVCodecID id, bool nalPrefixes = true);
    ~FrameTypeExtractor();

    enum FrameType { UnknownFrameType, I_Frame, P_Frame, B_Frame };
    FrameType getFrameType(const quint8* data, int dataLen);

    QnConstMediaContextPtr getContext() const { return m_context; }
    AVCodecID getCodec() const { return m_codecId; }

private:
    FrameType getH264FrameType(const quint8* data, int size);
    FrameType getMpegVideoFrameType(const quint8* data, int size);
    FrameType getWMVFrameType(const quint8* data, int size);
    FrameType getVCFrameType(const quint8* data, int size);
    FrameType getMpeg4FrameType(const quint8* data, int size);

    void decodeWMVSequence(const quint8* data, int size);

private:
    QnConstMediaContextPtr m_context;

    AVCodecID m_codecId;
    VC1SequenceHeader* m_vcSequence;
    bool m_dataWithNalPrefixes;
};

#endif // FRAME_TYPE_EXTRACTOR_H



