#include "videostreamdisplay.h"
#include "../base/log.h"
#include "../decoders/video/abstractdecoder.h"
#include "../data/mediadata.h"
#include "abstractrenderer.h"
#include "gl_renderer.h"

CLVideoStreamDisplay::CLVideoStreamDisplay(bool canDownscale) :
    m_lightCPUmode(CLAbstractVideoDecoder::DecodeMode_Full),
    m_canDownscale(canDownscale),
    m_prevFactor(CLVideoDecoderOutput::factor_1),
    m_scaleFactor(CLVideoDecoderOutput::factor_1),
    m_previousOnScreenSize(0,0),
    m_frameRGBA(0),
    m_buffer(0),
    m_scaleContext(0),
    m_outputWidth(0),
    m_outputHeight(0),
    m_frameQueueIndex(0),
    m_enableFrameQueue(false),
    m_queueUsed(false),
    m_needReinitDecoders(false)
{
}

CLVideoStreamDisplay::~CLVideoStreamDisplay()
{
    QMutexLocker _lock(&m_mtx);

    foreach(CLAbstractVideoDecoder* decoder, m_decoder)
    {
        delete decoder;
    }

    freeScaleContext();
}

void CLVideoStreamDisplay::setDrawer(CLAbstractRenderer* draw)
{
    m_drawer = draw;
}

CLVideoDecoderOutput::downscale_factor CLVideoStreamDisplay::getCurrentDownscaleFactor() const
{
    return m_scaleFactor;
}

bool CLVideoStreamDisplay::allocScaleContext(const CLVideoDecoderOutput& outFrame, int newWidth, int newHeight)
{
    m_outputWidth = newWidth;
    m_outputHeight = newHeight;

    m_scaleContext = sws_getContext(outFrame.width, outFrame.height, (PixelFormat) outFrame.format,
                                    m_outputWidth, m_outputHeight, PIX_FMT_RGBA,
                                    SWS_POINT, NULL, NULL, NULL);

    if (m_scaleContext == 0)
    {
        cl_log.log(QLatin1String("Can't get swscale context"), cl_logERROR);
        return false;
    }

    m_frameRGBA = avcodec_alloc_frame();

    int numBytes = avpicture_get_size(PIX_FMT_RGBA, m_outputWidth, m_outputHeight);
    //m_outFrame.out_type = PIX_FMT_RGBA;

    m_buffer = (quint8*)av_malloc(numBytes * sizeof(quint8));

    avpicture_fill((AVPicture *)m_frameRGBA, m_buffer, PIX_FMT_RGBA, m_outputWidth, m_outputHeight);

    return true;
}

void CLVideoStreamDisplay::freeScaleContext()
{
	if (m_scaleContext) {
		sws_freeContext(m_scaleContext);
		av_free(m_buffer);
		av_free(m_frameRGBA);
		m_scaleContext = 0;
	}
}

CLVideoDecoderOutput::downscale_factor CLVideoStreamDisplay::determineScaleFactor(CLCompressedVideoData* data, 
                                                                                  int srcWidth, int srcHeight, 
                                                                                  CLVideoDecoderOutput::downscale_factor force_factor)
{
    if (m_drawer->constantDownscaleFactor())
       force_factor = CLVideoDecoderOutput::factor_2;

	if (force_factor==CLVideoDecoderOutput::factor_any) // if nobody pushing lets peek it
	{
		QSize on_screen = m_drawer->sizeOnScreen(data->channelNumber);

		m_scaleFactor = findScaleFactor(srcWidth, srcHeight, on_screen.width(), on_screen.height());

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
    int newWidth = srcWidth / rez;
    int newHeight = srcHeight / rez;
    int maxTextureSize = CLGLRenderer::getMaxTextureSize();
    while (maxTextureSize > 0 && newWidth > maxTextureSize || newHeight > maxTextureSize)
    {
        rez = CLVideoDecoderOutput::downscale_factor ((int)rez * 2);
        newWidth /= 2;
        newHeight /= 2;
    }
    return rez;
}
/*
void CLVideoStreamDisplay::waitUndisplayedFrames(int channelNumber)
{
    for (int i = 0; i < MAX_FRAME_QUEUE_SIZE; ++i)
        waitFrame(i, channelNumber);
}
*/

void CLVideoStreamDisplay::waitFrame(int i, int channelNumber)
{
    QMutexLocker lock(&m_drawer->getMutex());
    while (m_frameQueue[i].isDisplaying())
        m_drawer->waitForFrameDisplayed(channelNumber);
}

void CLVideoStreamDisplay::dispay(CLCompressedVideoData* data, bool draw, CLVideoDecoderOutput::downscale_factor force_factor)
{
    // use only 1 frame for non selected video
    bool enableFrameQueue = m_enableFrameQueue;
    if (!enableFrameQueue && m_queueUsed)
    {
        waitFrame(m_frameQueueIndex, data->channelNumber);
        m_frameQueueIndex = 0;
        for (int i = 1; i < MAX_FRAME_QUEUE_SIZE; ++i)
            m_frameQueue[i].clean();
        m_queueUsed = false;
    }
    if (m_needReinitDecoders) {
        QMutexLocker _lock(&m_mtx);
        foreach(CLAbstractVideoDecoder* decoder, m_decoder)
            decoder->setMTDecoding(enableFrameQueue);
        m_needReinitDecoders = false;
    }

    CLVideoDecoderOutput m_tmpFrame;
    m_tmpFrame.setUseExternalData(true);

	CLAbstractVideoDecoder* dec;
	{
		QMutexLocker mutex(&m_mtx);

		if (data->compressionType == CODEC_ID_NONE)
		{
			cl_log.log(QLatin1String("CLVideoStreamDisplay::dispay: unknown codec type..."), cl_logERROR);
			return;
		}

        dec = m_decoder[data->compressionType];
		if (dec == 0)
		{
			dec = CLVideoDecoderFactory::createDecoder(data->compressionType, data->context);
			dec->setLightCpuMode(m_lightCPUmode);
            m_decoder.insert(data->compressionType, dec);
		}
        if (dec == 0) {
            CL_LOG(cl_logDEBUG2) cl_log.log(QLatin1String("Can't find video decoder"), cl_logDEBUG2);
            return;
        }
	}

    CLVideoDecoderOutput::downscale_factor scaleFactor = determineScaleFactor(data, dec->getWidth(), dec->getHeight(), force_factor);
    PixelFormat pixFmt = dec->GetPixelFormat();
    bool useTmpFrame =  !CLGLRenderer::isPixelFormatSupported(pixFmt) ||
        !CLVideoDecoderOutput::isPixelFormatSupported(pixFmt) || 
        scaleFactor > CLVideoDecoderOutput::factor_1;

    if (enableFrameQueue) {
        m_frameQueueIndex = (m_frameQueueIndex + 1) % MAX_FRAME_QUEUE_SIZE; // allow frame queue for selected video
        m_queueUsed = true;
    }

    CLVideoDecoderOutput& outFrame = m_frameQueue[m_frameQueueIndex];
    if (!useTmpFrame)
        outFrame.setUseExternalData(!enableFrameQueue);
	if (!dec || !dec->decode(*data, useTmpFrame ? &m_tmpFrame : &outFrame))
	{
		CL_LOG(cl_logDEBUG2) cl_log.log(QLatin1String("CLVideoStreamDisplay::dispay: decoder cannot decode image..."), cl_logDEBUG2);
		return;
	}
    if (!data->context) {
        // for stiil images decoder parameters are not know before decoding, so recalculate parameters
        // It is got one more data copy from tmpFrame, but for movies we have got valid dst pointer (tmp or not) immediate before decoding
        // It is necessary for new logic with display queue
        scaleFactor = determineScaleFactor(data, dec->getWidth(), dec->getHeight(), force_factor);
        pixFmt = dec->GetPixelFormat();
    }

	if (!draw || !m_drawer)
		return;

    if (useTmpFrame)
    {
        if (CLGLRenderer::isPixelFormatSupported(pixFmt) && CLVideoDecoderOutput::isPixelFormatSupported(pixFmt) && scaleFactor <= CLVideoDecoderOutput::factor_8)
            CLVideoDecoderOutput::downscale(&m_tmpFrame, &outFrame, scaleFactor); // fast scaler
        else 
            rescaleFrame(m_tmpFrame, outFrame, m_tmpFrame.width / scaleFactor, m_tmpFrame.height / scaleFactor); // universal scaler
    }
    m_drawer->draw(&outFrame, data->channelNumber);

    if (!enableFrameQueue) 
        m_drawer->waitForFrameDisplayed(data->channelNumber);
}

bool CLVideoStreamDisplay::rescaleFrame(const CLVideoDecoderOutput& srcFrame, CLVideoDecoderOutput& outFrame, int newWidth, int newHeight)
{
    static const int ROUND_FACTOR = 16;
    // due to openGL requirements chroma MUST be devided by 4, luma MUST be devided by 8
    // due to MMX scaled functions requirements chroma MUST be devided by 8, so luma MUST be devided by 16
    newWidth = (newWidth / ROUND_FACTOR ) * ROUND_FACTOR;

    if (m_scaleContext != 0 && (m_outputWidth != newWidth || m_outputHeight != newHeight))
    {
        freeScaleContext();
        if (!allocScaleContext(srcFrame, newWidth, newHeight))
            return false;
    }
    else if (m_scaleContext == 0)
    {
        if (!allocScaleContext(srcFrame, newWidth, newHeight))
            return false;
    }

    //const quint8* const srcSlice[] = {srcFrame.C1, srcFrame.C2, srcFrame.C3};
    //int srcStride[] = {srcFrame.stride1, srcFrame.stride2, srcFrame.stride3};

    sws_scale(m_scaleContext,srcFrame.data, srcFrame.linesize, 0,
        srcFrame.height, m_frameRGBA->data, m_frameRGBA->linesize);

    outFrame.width = m_outputWidth;
    outFrame.height = m_outputHeight;

    outFrame.data[0] = m_frameRGBA->data[0];
    outFrame.data[1] = m_frameRGBA->data[1];
    outFrame.data[2] = m_frameRGBA->data[2];

    outFrame.linesize[0] = m_frameRGBA->linesize[0];
    outFrame.linesize[1] = m_frameRGBA->linesize[1];
    outFrame.linesize[2] = m_frameRGBA->linesize[2];
    outFrame.format = PIX_FMT_RGBA;
    return true;
}

void CLVideoStreamDisplay::setLightCPUMode(CLAbstractVideoDecoder::DecodeMode val)
{
	m_lightCPUmode = val;
	QMutexLocker mutex(&m_mtx);

    foreach(CLAbstractVideoDecoder* decoder, m_decoder)
    {
        decoder->setLightCpuMode(val);
    }
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
    m_enableFrameQueue = value;
    m_needReinitDecoders = true;
}
