#include "avi_strem_reader.h"
#include "device\device.h"
#include "avi_parser.h"

#include "libavformat/avformat.h"


CLAVIStreamReader::CLAVIStreamReader(CLDevice* dev ):
CLAbstractArchiveReader(dev),
m_videoStrmIndex(-1)
{

}

CLAVIStreamReader::~CLAVIStreamReader()
{

}


unsigned long CLAVIStreamReader::currTime() const
{
	return 0;
}



CLAbstractMediaData* CLAVIStreamReader::getNextData()
{
	return 0;
}

void CLAVIStreamReader::channeljumpTo(unsigned long msec, int channel)
{

}


bool CLAVIStreamReader::init()
{
	
	m_formatContext = avidll.avformat_alloc_context();

	int err = avidll.av_open_input_file(&m_formatContext, m_device->getUniqueId().toLatin1(), NULL, 0, NULL);
	if (err < 0)
	{
		destroy();
		return false; 
	}

	err = avidll.av_find_stream_info(m_formatContext);
	if (err < 0)
	{
		destroy();
		return false; 
	}


	for(unsigned i = 0; i < m_formatContext->nb_streams; i++) 
	{
		AVStream *strm= m_formatContext->streams[i];
		AVCodecContext *codecContext = strm->codec;

		if(codecContext->codec_type >= (unsigned)AVMEDIA_TYPE_NB)
			continue;

		switch(codecContext->codec_type) 
		{
			case AVMEDIA_TYPE_VIDEO:
			if (m_videoStrmIndex == -1) // Take only first video stream
				m_videoStrmIndex = i;
			break;
		default:
			break;
		}
	}

	// Alloc common resources

	avidll.av_init_packet(&m_packet);
	return true;
}

void CLAVIStreamReader::destroy()
{
	avidll.av_free(m_formatContext);
	m_formatContext = 0;

	mInited = false;
}
