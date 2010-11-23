#include "avi_strem_reader.h"
#include "device\device.h"
#include "avi_parser.h"

#include "libavformat/avformat.h"
#include "data/mediadata.h"




CLAVIStreamReader::CLAVIStreamReader(CLDevice* dev ):
CLAbstractArchiveReader(dev),
m_videoStrmIndex(-1)
{
	init();
}

CLAVIStreamReader::~CLAVIStreamReader()
{
	destroy();
}


unsigned long CLAVIStreamReader::currTime() const
{
	return 0;
}



CLAbstractMediaData* CLAVIStreamReader::getNextData()
{
	if (!m_formatContext || m_videoStrmIndex==-1)
		return 0;

	if (!getNextVideoPacket())
		return 0;

	if (m_codec_id!=CLAbstractMediaData::H264)
		m_codec_id = m_codec_id;

	CLCompressedVideoData* videoData = new CLCompressedVideoData(0,m_packet.size);
	CLByteArray& data = videoData->data;

	data.prepareToWrite(m_packet.size);
	memcpy(data.data(), m_packet.data, m_packet.size);
	data.done(m_packet.size);

	/*
	FILE* f = fopen("test.264_", "wb");
	fwrite(data.data(), 1, data.size(), f);
	fclose(f);
	/**/


	

	videoData->compressionType = m_codec_id;
	videoData->keyFrame = 1;//m_formatContext->streams[m_videoStrmIndex]->codec->coded_frame->key_frame;
	videoData->channel_num = 0;


	if (videoData->keyFrame)
		m_gotKeyFrame[0] = true;

	//=================
	if (isSingleShotMode())
		pause();

	msleep(20);

	avidll.av_free_packet(&m_packet);

	return videoData;

}

void CLAVIStreamReader::channeljumpTo(unsigned long msec, int channel)
{

}


bool CLAVIStreamReader::init()
{
	static bool firstInstance = true;

	if (firstInstance)
	{
		firstInstance = false;
		avidll.av_register_all();
	}
	
	m_formatContext = avidll.avformat_alloc_context();

	
	int err = avidll.av_open_input_file(&m_formatContext, m_device->getUniqueId().toLatin1().data(), NULL, 0, NULL);
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


	int ffmpeg_codec_id = m_formatContext->streams[m_videoStrmIndex]->codec->codec_id;

	switch(ffmpeg_codec_id)
	{
	case CODEC_ID_MJPEG:
		m_codec_id =CLAbstractMediaData::JPEG;
		break;

	case CODEC_ID_MPEG2VIDEO:
		m_codec_id = CLAbstractMediaData::MPEG2;
		break;


	case CODEC_ID_MPEG4:
		m_codec_id = CLAbstractMediaData::MPEG4;
		break;

	case CODEC_ID_H264:
		m_codec_id = CLAbstractMediaData::H264;
	    break;
	default:
		destroy();
		return false;
	}
	
	

	// Alloc common resources

	avidll.av_init_packet(&m_packet);
	return true;
}

void CLAVIStreamReader::destroy()
{
	avidll.av_free(m_formatContext);
	m_formatContext = 0;
}


bool CLAVIStreamReader::getNextVideoPacket()
{
	while(1)
	{
		int err = avidll.av_read_frame(m_formatContext, &m_packet);
		if (err < 0)
		{
			if (err == AVERROR_EOF)
			{
				destroy();
				init();

				err = avidll.av_read_frame(m_formatContext, &m_packet);
				if (err<0)
					return false;

			}
			else
				return false;
		}

		if (m_packet.stream_index != m_videoStrmIndex)
		{
			avidll.av_free_packet(&m_packet);
			continue;
		}

		break;

	}

	return true;

}