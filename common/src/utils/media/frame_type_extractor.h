#ifndef FRAME_TYPE_EXTRACTOR_H
#define FRAME_TYPE_EXTRACTOR_H

#ifdef ENABLE_DATA_PROVIDERS

class VC1SequenceHeader;

// ffmpeg library.
extern "C"
{
	// For enum CodecID, struct AVCodecContext.
    #include <libavcodec/avcodec.h>
}

class FrameTypeExtractor
{
public:
	FrameTypeExtractor(AVCodecContext* context);
	FrameTypeExtractor(CodecID id, bool nalPrefixes = true);
	~FrameTypeExtractor();

	enum FrameType {UnknownFrameType, I_Frame, P_Frame, B_Frame};
	FrameType getFrameType(const quint8* data, int dataLen);

	AVCodecContext* getContext() const { return m_context; }
    CodecID getCodec() const { return m_codecId; }

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

#endif // ENABLE_DATA_PROVIDERS

#endif // FRAME_TYPE_EXTRACTOR_H



