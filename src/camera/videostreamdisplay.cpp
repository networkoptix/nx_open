#include "videostreamdisplay.h"
#include "../base/log.h"
#include "../decoders/video/abstractdecoder.h"
#include "../data/mediadata.h"
#include "abstractrenderer.h"

PixelFormat pixelFormatFromColorSpace(CLColorSpace colorSpace)
{
	PixelFormat result;

	switch(colorSpace)
	{
	case CL_DECODER_YUV422:
		result = PIX_FMT_YUVJ422P;
		break;
	case CL_DECODER_YUV444:
		result = PIX_FMT_YUVJ444P;
		break;
	case CL_DECODER_YUV420:
		result = PIX_FMT_YUV420P;
		break;
	case CL_DECODER_RGB555LE:
		result = PIX_FMT_RGB555LE;
		break;
	default:
		result = PIX_FMT_YUV420P;
		break;
	}

	return result;
}

CLVideoStreamDisplay::CLVideoStreamDisplay(bool canDownscale)
    : m_lightCPUmode(false),
      m_canDownscale(canDownscale),
      m_prevFactor(CLVideoDecoderOutput::factor_1),
      m_scaleFactor(CLVideoDecoderOutput::factor_1),
      m_previousOnScreenSize(0,0),
	  m_scaleContext(0),
	  m_frameYUV(0),
	  m_buffer(0),
	  m_outputWidth(0),
	  m_outputHeight(0)
{
	for (int i = 0; i < CL_VARIOUSE_DECODERS;++i)
		m_decoder[i] = 0;
}

CLVideoStreamDisplay::~CLVideoStreamDisplay()
{
	for (int i = 0; i < CL_VARIOUSE_DECODERS;++i)
		delete m_decoder[i];

	freeScaleContext();
}

void CLVideoStreamDisplay::setDrawer(CLAbstractRenderer* draw)
{
	m_draw = draw;
}

CLVideoDecoderOutput::downscale_factor CLVideoStreamDisplay::getCurrentDownscaleFactor() const
{
	return m_scaleFactor;
}

void CLVideoStreamDisplay::allocScaleContext(const CLVideoDecoderOutput& outFrame)
{
	m_outputWidth = outFrame.width / m_scaleFactor;
	m_outputHeight = outFrame.height / m_scaleFactor;

	m_scaleContext = sws_getContext(outFrame.width, outFrame.height, pixelFormatFromColorSpace(outFrame.out_type),
		m_outputWidth, m_outputHeight, PIX_FMT_YUV420P,
		SWS_POINT, NULL, NULL, NULL);

	m_frameYUV = avcodec_alloc_frame();

	int numBytes = avpicture_get_size(PIX_FMT_YUV420P, m_outputWidth, m_outputHeight);
	m_outFrame.out_type = CL_DECODER_YUV420;

	m_buffer = (uint8_t*)av_malloc(numBytes * sizeof(uint8_t));

	avpicture_fill((AVPicture *)m_frameYUV, m_buffer, PIX_FMT_YUV420P, m_outputWidth, m_outputHeight);
}

void CLVideoStreamDisplay::freeScaleContext()
{
	if (m_scaleContext) {
		sws_freeContext(m_scaleContext);
		av_free(m_buffer);
		av_free(m_frameYUV);
        m_scaleContext = 0;
	}
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
			//if (on_screen.width()*8 <= img.outFrame.width  && on_screen.height()*8 <= img.outFrame.height) // never use 8 factor ( to low quality )
			//	m_scaleFactor = CLVideoDecoderOutput::factor_8;
			if (on_screen.width()*4 <= img.outFrame.width  && on_screen.height()*4 <= img.outFrame.height)
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

		//m_scaleFactor  = CLVideoDecoderOutput::factor_1;

		// XXX RGB555 hack. Need to refactor video processing code.
		if (m_scaleFactor == CLVideoDecoderOutput::factor_1 && img.outFrame.out_type != CL_DECODER_RGB555LE)
		{
			m_draw->draw(img.outFrame, data->channelNumber);
		}
		else
		{
			if (img.outFrame.out_type == CL_DECODER_RGB555LE)
			{
				int newWidth = img.outFrame.width / m_scaleFactor;
				int newHeight = img.outFrame.height / m_scaleFactor;

				if (m_scaleContext != 0 && (m_outputWidth != newWidth || m_outputHeight != newHeight))
				{
					freeScaleContext();
					allocScaleContext(img.outFrame);
				} else if (m_scaleContext == 0)
				{
					allocScaleContext(img.outFrame);
				}

				const uint8_t* const srcSlice[] = {img.outFrame.C1, img.outFrame.C2, img.outFrame.C3};
				int srcStride[] = {img.outFrame.stride1, img.outFrame.stride2, img.outFrame.stride3};

				sws_scale(m_scaleContext,srcSlice, srcStride, 0, 
					img.outFrame.height, 
					m_frameYUV->data, m_frameYUV->linesize);

				m_outFrame.width = m_outputWidth;
				m_outFrame.height = m_outputHeight;

				m_outFrame.C1 = m_frameYUV->data[0];
				m_outFrame.C2 = m_frameYUV->data[1];
				m_outFrame.C3 = m_frameYUV->data[2];

				m_outFrame.stride1 = m_frameYUV->linesize[0];
				m_outFrame.stride2 = m_frameYUV->linesize[1];
				m_outFrame.stride3 = m_frameYUV->linesize[2];
			} else 
			{
				CLVideoDecoderOutput::downscale(&img.outFrame, &m_outFrame, m_scaleFactor); // extra cpu work but less to display( for weak video cards )
			}
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
