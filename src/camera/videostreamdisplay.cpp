#include "videostreamdisplay.h"
#include "../base/log.h"
#include "../decoders/video/abstractdecoder.h"
#include "../data/mediadata.h"
#include "abstractrenderer.h"
#include <QMutexLocker>



CLVideoStreamDisplay::CLVideoStreamDisplay():
m_lightCPUmode(false)
{
	for (int i = 0; i < CL_VARIOUSE_DECODERS;++i)
		m_decoder[i] = 0;
}

CLVideoStreamDisplay::~CLVideoStreamDisplay()
{
	for (int i = 0; i < CL_VARIOUSE_DECODERS;++i)
		delete m_decoder[i];

}

void CLVideoStreamDisplay::setDrawer(CLAbstractRenderer* draw)
{
	m_draw = draw;
}

void CLVideoStreamDisplay::dispay(CLCompressedVideoData* data)
{

	CLVideoData img;

	img.inbuf = (unsigned char*)(data->data.data()); // :-)
	img.buff_len = data->data.size();
	img.key_frame = data->keyFrame;
	img.use_twice = data->use_twice;
	CLAbstractVideoDecoder* dec;


	{

		QMutexLocker mutex(&m_mtx);


		if (data->compressionType<0 || data->compressionType>CL_VARIOUSE_DECODERS-1)
		{
			cl_log.log("CLVideoStreamDisplay::dispay: unknown codec type...", cl_logERROR);
			return;
		}

		if (m_decoder[data->compressionType]==0)
		{
			m_decoder[data->compressionType] = CLVideoDecoderFactory::createDecoder(data->compressionType);
			m_decoder[data->compressionType]->setLightCpuMode(m_lightCPUmode);
		}

		img.codec = data->compressionType;
		dec = m_decoder[data->compressionType];


	}


	if (dec && dec->decode(img))
	{
		if (m_draw)
		{
			/**/
			QSize on_screen = m_draw->size_on_screen(data->channel_num);
			if (on_screen.width()*4 <= img.out_frame.width  && on_screen.height()*4 <= img.out_frame.height)
			{
				CLVideoDecoderOutput::downscale_factor4(&img.out_frame, &m_out_frame4); // extra cpu work but less to display( for weal video cards )
				m_draw->draw(m_out_frame4, data->channel_num);
			}
			else if (on_screen.width()*2 <= img.out_frame.width  && on_screen.height()*2 <= img.out_frame.height)
			{
				CLVideoDecoderOutput::downscale_factor2(&img.out_frame, &m_out_frame2); // extra cpu work but less to display( for weal video cards )
				m_draw->draw(m_out_frame2, data->channel_num);
			}
			else
			/**/
				m_draw->draw(img.out_frame, data->channel_num);
		}
	}
	else
	{
		CL_LOG(cl_logDEBUG2) cl_log.log("CLVideoStreamDisplay::dispay: decoder cannot decode image...", cl_logDEBUG2);
	}
}

void CLVideoStreamDisplay::setLightCPUMode(bool val)
{
	m_lightCPUmode = val;
	QMutexLocker mutex(&m_mtx);

	for (int i = 0; i < CL_VARIOUSE_DECODERS;++i)
		if (m_decoder[i])
			m_decoder[i]->setLightCpuMode(val);
}

void CLVideoStreamDisplay::coppyImage(bool copy)
{
	m_draw->copyVideoDataBeforePainting(copy);
}