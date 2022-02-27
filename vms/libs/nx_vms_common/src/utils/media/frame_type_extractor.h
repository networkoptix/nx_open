// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef FRAME_TYPE_EXTRACTOR_H
#define FRAME_TYPE_EXTRACTOR_H

class VC1SequenceHeader;

extern "C"
{
// For enum AVCodecID, struct AVCodecContext.
#include <libavcodec/avcodec.h>
}

#include <nx/streaming/av_codec_media_context.h>

class NX_VMS_COMMON_API FrameTypeExtractor
{
public:
    FrameTypeExtractor(const CodecParametersConstPtr& context);
    FrameTypeExtractor(AVCodecID id, bool nalPrefixes = true);
    ~FrameTypeExtractor();

    enum FrameType { UnknownFrameType, I_Frame, P_Frame, B_Frame };
    FrameType getFrameType(const quint8* data, int dataLen);

    CodecParametersConstPtr getContext() const { return m_context; }
    AVCodecID getCodec() const { return m_codecId; }

private:
    FrameType getH264FrameType(const quint8* data, int size);
    FrameType getMpegVideoFrameType(const quint8* data, int size);
    FrameType getWMVFrameType(const quint8* data, int size);
    FrameType getVCFrameType(const quint8* data, int size);
    FrameType getMpeg4FrameType(const quint8* data, int size);

    void decodeWMVSequence(const quint8* data, int size);

private:
    CodecParametersConstPtr m_context;

    AVCodecID m_codecId;
    VC1SequenceHeader* m_vcSequence;
    bool m_dataWithNalPrefixes;
};

#endif // FRAME_TYPE_EXTRACTOR_H



