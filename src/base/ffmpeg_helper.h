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
};


#endif // __FFMPEG_HELPER_H
