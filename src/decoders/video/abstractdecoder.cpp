#include "abstractdecoder.h"
#include "ffmpeg.h"
#include "ipp_h264_decoder.h"

CLVideoDecoderFactory::CLCodecManufacture CLVideoDecoderFactory::m_codecManufacture = FFMPEG;

CLAbstractVideoDecoder* CLVideoDecoderFactory::createDecoder(CodecID codec, void* context)
{

	if (codec == CODEC_ID_MJPEG)
		return new CLFFmpegVideoDecoder(CODEC_ID_MJPEG); // for now only FFMPEG JPEG decoder is ready

	// h264 
	switch(m_codecManufacture)
	{
#ifdef _WIN32
	case INTELIPP:
		return new IPPH264Decoder();
	    break;
#endif
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
      m_hardwareAccelerationEnabled(false),
      m_mtDecoding(false),
      m_needRecreate(false)
{}

bool CLAbstractVideoDecoder::isHardwareAccelerationEnabled() const 
{
    return m_hardwareAccelerationEnabled;
}
