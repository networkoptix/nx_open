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
	CLAbstractVideoDecoder* dec;


	{

		QMutexLocker mutex(&m_mtx);

		switch(data->compressionType)
		{
		case CLCompressedVideoData::JPEG:
			if (m_decoder[CL_JPEG]==0)
			{
				m_decoder[CL_JPEG] = CLDecoderFactory::createDecoder(CL_JPEG);
				m_decoder[CL_JPEG]->setLightCpuMode(m_lightCPUmode);
			}

			img.codec = CL_JPEG;

			dec = m_decoder[CL_JPEG];


			break;
		case CLCompressedVideoData::H264:
			if (m_decoder[CL_H264]==0)
			{
				m_decoder[CL_H264] = CLDecoderFactory::createDecoder(CL_H264);
				m_decoder[CL_H264]->setLightCpuMode(m_lightCPUmode);
			}

			img.codec = CL_H264;
			dec = m_decoder[CL_H264];

			break;
		default:
			cl_log.log("CLVideoStreamDisplay::dispay: unknown codec type...", cl_logERROR);
			return;
			break;
		}
	}


	if (dec->decode(img))
	{
		if (m_draw)	
			m_draw->draw(img.out_frame, data->channel_num);
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