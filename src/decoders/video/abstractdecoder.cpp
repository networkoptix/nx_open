#include "abstractdecoder.h"
#include "ffmpeg.h"
#include "ipp_h264_decoder.h"

CLDecoderFactory::CLCodecManufacture CLDecoderFactory::m_codecManufacture = FFMPEG;

CLAbstractVideoDecoder* CLDecoderFactory::createDecoder(CLCodecType codec)
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
		return new CLFFmpegVideoDecoder(codec);
	    break;
	}
}