#ifndef clabstractdecoder_h_2155
#define clabstractdecoder_h_2155

#include "frame_info.h"

class CLAbstractVideoDecoder
{
public:
	explicit CLAbstractVideoDecoder();

	virtual ~CLAbstractVideoDecoder(){};

    /**
      * Decode video frame.
      * Set hardwareAccelerationEnabled flag if hardware acceleration was used
      */
	virtual bool decode(CLVideoData& )=0;

	virtual void showMotion(bool show ) = 0;

	virtual void setLightCpuMode(bool val) = 0;

    void setTryHardwareAcceleration(bool tryHardwareAcceleration);
    bool isHardwareAccelerationEnabled() const;
private:
	CLAbstractVideoDecoder(const CLAbstractVideoDecoder&){};
	CLAbstractVideoDecoder& operator=(CLAbstractVideoDecoder&){};

protected:
    bool m_tryHardwareAcceleration;
    bool m_hardwareAccelerationEnabled;
};

class CLVideoDecoderFactory
{
public:
	enum CLCodecManufacture{FFMPEG, INTELIPP};
	static CLAbstractVideoDecoder* createDecoder(CLCodecType codec, void* context);
	static void setCodecManufacture(CLCodecManufacture codecman)
	{
		m_codecManufacture = codecman;
	}
private:
	static CLCodecManufacture m_codecManufacture;
};

#endif //clabstractdecoder_h_2155