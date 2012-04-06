#include "abstractdecoder.h"
#include "ffmpeg.h"
#include "ipp_h264_decoder.h"

CLVideoDecoderFactory::CLCodecManufacture CLVideoDecoderFactory::m_codecManufacture = FFMPEG;

QnAbstractVideoDecoder* CLVideoDecoderFactory::createDecoder(const QnCompressedVideoDataPtr data)
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

void QnAbstractVideoDecoder::setTryHardwareAcceleration(bool tryHardwareAcceleration)
{
    m_tryHardwareAcceleration = tryHardwareAcceleration;
}

QnAbstractVideoDecoder::QnAbstractVideoDecoder()
    : m_tryHardwareAcceleration(false),
      m_hardwareAccelerationEnabled(false),
      m_mtDecoding(false),
      m_needRecreate(false)
{}

bool QnAbstractVideoDecoder::isHardwareAccelerationEnabled() const 
{
    return m_hardwareAccelerationEnabled;
}
