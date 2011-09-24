#include "abstractdecoder.h"
#include "ffmpeg.h"
#include "ipp_h264_decoder.h"

CLVideoDecoderFactory::CLCodecManufacture CLVideoDecoderFactory::m_codecManufacture = FFMPEG;

CLAbstractVideoDecoder* CLVideoDecoderFactory::createDecoder(const CLCompressedVideoData* data)
{

	// h264 
	switch(m_codecManufacture)
	{
#ifdef Q_OS_WIN
	case INTELIPP:
		return new IPPH264Decoder();
	    break;
#endif
	case FFMPEG:
	default:
		return new CLFFmpegVideoDecoder(data->compressionType, data);
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
