#include "videostreamdisplay.h"
#include "../base/log.h"
#include "../decoders/video/abstractdecoder.h"
#include "../data/mediadata.h"
#include "abstractrenderer.h"
#include "gl_renderer.h"

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
}

CLVideoStreamDisplay::~CLVideoStreamDisplay()
{
    foreach(CLAbstractVideoDecoder* decoder, m_decoder)
    {
        delete decoder;
    }

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

bool CLVideoStreamDisplay::allocScaleContext(const CLVideoDecoderOutput& outFrame, int newWidth, int newHeight)
{
    m_outputWidth = newWidth;
    m_outputHeight = newHeight;

	m_scaleContext = sws_getContext(outFrame.width, outFrame.height, outFrame.out_type,
		m_outputWidth, m_outputHeight, PIX_FMT_YUV420P,
		SWS_POINT, NULL, NULL, NULL);

    if (m_scaleContext == 0)
    {
        cl_log.log("Can't get swscale context", cl_logERROR);
        return false;
    }

	m_frameYUV = avcodec_alloc_frame();

	int numBytes = avpicture_get_size(PIX_FMT_YUV420P, m_outputWidth, m_outputHeight);
	m_outFrame.out_type = PIX_FMT_YUV420P;

	m_buffer = (uint8_t*)av_malloc(numBytes * sizeof(uint8_t));

	avpicture_fill((AVPicture *)m_frameYUV, m_buffer, PIX_FMT_YUV420P, m_outputWidth, m_outputHeight);

    return true;
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

CLVideoDecoderOutput::downscale_factor CLVideoStreamDisplay::determineScaleFactor(CLCompressedVideoData* data, const CLVideoData& img, CLVideoDecoderOutput::downscale_factor force_factor)
{
    if (m_draw->constantDownscaleFactor())
       force_factor = CLVideoDecoderOutput::factor_2;

	if (force_factor==CLVideoDecoderOutput::factor_any) // if nobody pushing lets peek it 
	{
		QSize on_screen = m_draw->sizeOnScreen(data->channelNumber);

        m_scaleFactor = findScaleFactor(img.outFrame.width, img.outFrame.height, on_screen.width(), on_screen.height());

		if (m_scaleFactor < m_prevFactor)
		{
			// new factor is less than prev one; about to change factor => about to increase resource usage 
			if ( qAbs((qreal)on_screen.width() - m_previousOnScreenSize.width())/on_screen.width() < 0.05 && 
				qAbs((qreal)on_screen.height() - m_previousOnScreenSize.height())/on_screen.height() < 0.05)
            {
				m_scaleFactor = m_prevFactor; // hold bigger factor ( smaller image )
            }

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

    CLVideoDecoderOutput::downscale_factor rez = m_canDownscale ? qMax(m_scaleFactor, CLVideoDecoderOutput::factor_1) : CLVideoDecoderOutput::factor_1;
    // If there is no scaling needed check if size is greater than maximum allowed image size (maximum texture size for opengl).
    int newWidth = img.outFrame.width / m_scaleFactor;
    int newHeight = img.outFrame.height / m_scaleFactor;
    int maxTextureSize = CLGLRenderer::getMaxTextureSize();
    while (maxTextureSize > 0 && newWidth > maxTextureSize || newHeight > maxTextureSize)
    {
        rez = CLVideoDecoderOutput::downscale_factor ((int)rez * 2);
        newWidth /= 2;
        newHeight /= 2;
    }
    return rez;
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

		if (data->compressionType == CODEC_ID_NONE)
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

    int maxTextureSize = CLGLRenderer::getMaxTextureSize();

    CLVideoDecoderOutput::downscale_factor scaleFactor = determineScaleFactor(data, img, force_factor);

    if (!CLGLRenderer::isPixelFormatSupported(img.outFrame.out_type) || 
        scaleFactor > CLVideoDecoderOutput::factor_8 ||
        (scaleFactor > CLVideoDecoderOutput::factor_1 && !CLVideoDecoderOutput::isPixelFormatSupported(img.outFrame.out_type)))
    {
        rescaleFrame(img.outFrame, img.outFrame.width / m_scaleFactor, img.outFrame.height / m_scaleFactor);
        m_draw->draw(m_outFrame, data->channelNumber);
    }
    else if (scaleFactor > CLVideoDecoderOutput::factor_1)
    {
        CLVideoDecoderOutput::downscale(&img.outFrame, &m_outFrame, scaleFactor); // extra cpu work but less to display( for weak video cards )
        m_draw->draw(m_outFrame, data->channelNumber);
    }
    else {
        m_draw->draw(img.outFrame, data->channelNumber);
    }
}

bool CLVideoStreamDisplay::rescaleFrame(CLVideoDecoderOutput& outFrame, int newWidth, int newHeight)
{
    static const int ROUND_FACTOR = 16;
    // due to openGL requirements chroma MUST be devided by 4, luma MUST be devided by 8
    // due to MMX scaled functions requirements chroma MUST be devided by 8, so luma MUST be devided by 16
    newWidth = (newWidth / ROUND_FACTOR ) * ROUND_FACTOR;

    if (m_scaleContext != 0 && (m_outputWidth != newWidth || m_outputHeight != newHeight))
    {
        freeScaleContext();
        if (!allocScaleContext(outFrame, newWidth, newHeight))
            return false;
    }
    else if (m_scaleContext == 0)
    {
        if (!allocScaleContext(outFrame, newWidth, newHeight))
            return false;
    }

    const uint8_t* const srcSlice[] = {outFrame.C1, outFrame.C2, outFrame.C3};
    int srcStride[] = {outFrame.stride1, outFrame.stride2, outFrame.stride3};

    sws_scale(m_scaleContext,srcSlice, srcStride, 0, 
        outFrame.height, m_frameYUV->data, m_frameYUV->linesize);

    m_outFrame.width = m_outputWidth;
    m_outFrame.height = m_outputHeight;

    m_outFrame.C1 = m_frameYUV->data[0];
    m_outFrame.C2 = m_frameYUV->data[1];
    m_outFrame.C3 = m_frameYUV->data[2];

    m_outFrame.stride1 = m_frameYUV->linesize[0];
    m_outFrame.stride2 = m_frameYUV->linesize[1];
    m_outFrame.stride3 = m_frameYUV->linesize[2];

    return true;
}

void CLVideoStreamDisplay::setLightCPUMode(bool val)
{
	m_lightCPUmode = val;
	QMutexLocker mutex(&m_mtx);

    foreach(CLAbstractVideoDecoder* decoder, m_decoder)
    {
	    decoder->setLightCpuMode(val);
    }
}

void CLVideoStreamDisplay::copyImage(bool copy)
{
	m_draw->copyVideoDataBeforePainting(copy);
}

CLVideoDecoderOutput::downscale_factor CLVideoStreamDisplay::findScaleFactor(int width, int height, int fitWidth, int fitHeight)
{
    if (fitWidth * 8 <= width  && fitHeight * 8 <= height) 
        return CLVideoDecoderOutput::factor_8;
    if (fitWidth * 4 <= width  && fitHeight * 4 <= height)
        return CLVideoDecoderOutput::factor_4;
    else if (fitWidth * 2 <= width  && fitHeight * 2 <= height)
        return CLVideoDecoderOutput::factor_2;
    else
        return CLVideoDecoderOutput::factor_1;
}

void CLVideoStreamDisplay::setMTDecoding(bool value)
{
    foreach(CLAbstractVideoDecoder* decoder, m_decoder)
    {
        decoder->setMTDecoding(value);
    }
}

