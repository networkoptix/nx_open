#ifndef clabstractdecoder_h_2155
#define clabstractdecoder_h_2155

#include "frame_info.h"

class CLAbstractVideoDecoder
{
public:
    // for movies: full = IPB, fast == IP only, fastest = I only
    enum DecodeMode {DecodeMode_NotDefined, DecodeMode_Full, DecodeMode_Fast, DecodeMode_Fastest};

	explicit CLAbstractVideoDecoder();

	virtual ~CLAbstractVideoDecoder(){};

    virtual PixelFormat GetPixelFormat() { return PIX_FMT_NONE; }

    /**
      * Decode video frame.
      * Set hardwareAccelerationEnabled flag if hardware acceleration was used
      */
    virtual bool decode(const CLCompressedVideoData& data, CLVideoDecoderOutput* outFrame) = 0;

	virtual void showMotion(bool show ) = 0;

	virtual void setLightCpuMode(DecodeMode val) = 0;

    void setMTDecoding(bool value)
    {
        if (m_mtDecoding != value)
            m_needRecreate = true;

        m_mtDecoding = value;
    }


    void setTryHardwareAcceleration(bool tryHardwareAcceleration);
    bool isHardwareAccelerationEnabled() const;
    virtual int getWidth() const  { return 0; }
    virtual int getHeight() const { return 0; }
    virtual double getSampleAspectRatio() const { return 1; };
    virtual PixelFormat getFormat() const { return PIX_FMT_YUV420P; }
    virtual void flush() {}
    virtual const AVFrame* lastFrame() { return 0; }
    virtual void resetDecoder() {}
private:
	CLAbstractVideoDecoder(const CLAbstractVideoDecoder&) {}
	CLAbstractVideoDecoder& operator=(const CLAbstractVideoDecoder&) { return *this; }

protected:
    bool m_tryHardwareAcceleration;
    bool m_hardwareAccelerationEnabled;

    bool m_mtDecoding;
    bool m_needRecreate;
};

class CLVideoDecoderFactory
{
public:
	enum CLCodecManufacture{FFMPEG, INTELIPP};
	static CLAbstractVideoDecoder* createDecoder(CodecID codec, void* context);
	static void setCodecManufacture(CLCodecManufacture codecman)
	{
		m_codecManufacture = codecman;
	}
private:
	static CLCodecManufacture m_codecManufacture;
};

#endif //clabstractdecoder_h_2155
