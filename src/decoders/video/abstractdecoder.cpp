#include "abstractdecoder.h"
#include "ffmpeg.h"
#include "ipp_h264_decoder.h"

CLVideoDecoderFactory::CLCodecManufacture CLVideoDecoderFactory::m_codecManufacture = FFMPEG;

CLAbstractVideoDecoder* CLVideoDecoderFactory::createDecoder(CLCodecType codec, void* context)
{

	if (codec == CL_JPEG)
		return new CLFFmpegVideoDecoder(CL_JPEG); // for now only FFMPEG JPEG decoder is ready

	// h264 
	switch(m_codecManufacture)
	{
	case INTELIPP:
		return new IPPH264Decoder();
	    break;
	case FFMPEG:
	default:
		return new CLFFmpegVideoDecoder(codec, (AVCodecContext*)context);
	    break;
	}

	return 0;
}

void CLAbstractVideoDecoder::setTryHardwareAcceleration(bool tryHardwareAcceleration)
{
    m_tryHardwareAcceleration = tryHardwareAcceleration;
}

CLAbstractVideoDecoder::CLAbstractVideoDecoder()
    : m_tryHardwareAcceleration(false),
      m_hardwareAccelerationEnabled(false)
{}

bool CLAbstractVideoDecoder::isHardwareAccelerationEnabled() const 
{
    return m_hardwareAccelerationEnabled;
}
