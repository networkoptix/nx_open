#include "videostreamdisplay.h"
#include "../base/log.h"
#include "../decoders/video/abstractdecoder.h"
#include "../data/mediadata.h"
#include "abstractrenderer.h"

CLVideoStreamDisplay::CLVideoStreamDisplay(bool canDownscale)
    : m_lightCPUmode(false),
      m_canDownscale(canDownscale),
      m_prevFactor(CLVideoDecoderOutput::factor_1),
      m_scaleFactor(CLVideoDecoderOutput::factor_1),
      m_previousOnScreenSize(0,0)
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

CLVideoDecoderOutput::downscale_factor CLVideoStreamDisplay::getCurrentDownscaleFactor() const
{
	return m_scaleFactor;
}

void CLVideoStreamDisplay::dispay(CLCompressedVideoData* data, bool draw, CLVideoDecoderOutput::downscale_factor force_factor)
{
	CLVideoData img;

	img.inBuffer = (unsigned char*)(data->data.data()); // :-)
	img.bufferLength = data->data.size();
	img.keyFrame = data->keyFrame;
	img.useTwice = data->useTwice;
    img.width = data->width;
    img.height = data->height;

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
			m_decoder[data->compressionType] = CLVideoDecoderFactory::createDecoder(data->compressionType, data->context);
			m_decoder[data->compressionType]->setLightCpuMode(m_lightCPUmode);
		}

		img.codec = data->compressionType;
		dec = m_decoder[data->compressionType];
	}

	if (!dec || !dec->decode(img))
	{
		CL_LOG(cl_logDEBUG2) cl_log.log("CLVideoStreamDisplay::dispay: decoder cannot decode image...", cl_logDEBUG2);
		return;
	}

	if (!draw || !m_draw)
		return;

	if (m_canDownscale)
	{
		//force_factor
		m_scaleFactor = CLVideoDecoderOutput::factor_1;

		if (force_factor==CLVideoDecoderOutput::factor_any) // if nobody pushing lets peek it 
		{
			QSize on_screen = m_draw->sizeOnScreen(data->channelNumber);
			if (on_screen.width()*8 <= img.outFrame.width  && on_screen.height()*8 <= img.outFrame.height)
				m_scaleFactor = CLVideoDecoderOutput::factor_8;
			else if (on_screen.width()*4 <= img.outFrame.width  && on_screen.height()*4 <= img.outFrame.height)
				m_scaleFactor = CLVideoDecoderOutput::factor_4;
			else if (on_screen.width()*2 <= img.outFrame.width  && on_screen.height()*2 <= img.outFrame.height)
				m_scaleFactor = CLVideoDecoderOutput::factor_2;

			if (m_scaleFactor < m_prevFactor)
			{
				// new factor is less than prev one; about to change factor => about to increase resource usage 
				if ( qAbs((qreal)on_screen.width() - m_previousOnScreenSize.width())/on_screen.width() < 0.05 && 
					qAbs((qreal)on_screen.height() - m_previousOnScreenSize.height())/on_screen.height() < 0.05)
					m_scaleFactor = m_prevFactor; // hold bigger factor ( smaller image )

				// why?
				// we need to do so ( introduce some histerezis )coz downscaling changes resolution not proportionally some time( cut vertical size a bit )
				// so it may be a loop downscale => changed aspectratio => upscale => changed aspectratio => downscale.
			}

			if (m_scaleFactor != m_prevFactor)
			{
				m_previousOnScreenSize = on_screen;
				m_prevFactor = m_scaleFactor;
			}

		}
		else
			m_scaleFactor = force_factor;

		//factor = CLVideoDecoderOutput::factor_1;

		if (m_scaleFactor == CLVideoDecoderOutput::factor_1)
		{
			m_draw->draw(img.outFrame, data->channelNumber);
		}
		else
		{
			CLVideoDecoderOutput::downscale(&img.outFrame, &m_outFrame, m_scaleFactor); // extra cpu work but less to display( for weak video cards )
			m_draw->draw(m_outFrame, data->channelNumber);
		}

	}
	else
		m_draw->draw(img.outFrame, data->channelNumber);
}

void CLVideoStreamDisplay::setLightCPUMode(bool val)
{
	m_lightCPUmode = val;
	QMutexLocker mutex(&m_mtx);

	for (int i = 0; i < CL_VARIOUSE_DECODERS;++i)
		if (m_decoder[i])
			m_decoder[i]->setLightCpuMode(val);
}

void CLVideoStreamDisplay::copyImage(bool copy)
{
	m_draw->copyVideoDataBeforePainting(copy);
}
