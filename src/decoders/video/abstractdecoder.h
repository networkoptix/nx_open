#ifndef clabstractdecoder_h_2155
#define clabstractdecoder_h_2155

#include "frame_info.h"

class CLAbstractVideoDecoder
{
public:
	explicit CLAbstractVideoDecoder(){}
	virtual ~CLAbstractVideoDecoder(){};

	virtual bool decode(CLVideoData& )=0;

	virtual void showMotion(bool show ) = 0;

	virtual void setLightCpuMode(bool val) = 0;

private:
	CLAbstractVideoDecoder(const CLAbstractVideoDecoder&){};
	CLAbstractVideoDecoder& operator=(CLAbstractVideoDecoder&){};

};

class CLDecoderFactory
{
public:
	enum CLCodecManufacture{FFMPEG, INTELIPP};
	static CLAbstractVideoDecoder* createDecoder(CLCodecType codec);
	static void setCodecManufacture(CLCodecManufacture codecman)
	{
		m_codecManufacture = codecman;
	}
private:
	static CLCodecManufacture m_codecManufacture;
};

#endif //clabstractdecoder_h_2155