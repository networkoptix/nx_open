#include "videostreamdisplay.h"
#include "../base/log.h"
#include "../decoders/video/abstractdecoder.h"
#include "../data/mediadata.h"
#include "abstractrenderer.h"
#include "gl_renderer.h"
#include "util.h"

CLVideoStreamDisplay::CLVideoStreamDisplay(bool canDownscale) :
    m_lightCPUmode(CLAbstractVideoDecoder::DecodeMode_Full),
    m_canDownscale(canDownscale),
    m_prevFactor(CLVideoDecoderOutput::factor_1),
    m_scaleFactor(CLVideoDecoderOutput::factor_1),
    m_previousOnScreenSize(0,0),
    m_scaleContext(0),
    m_outputWidth(0),
    m_outputHeight(0),
    m_frameQueueIndex(0),
    m_enableFrameQueue(false),
    m_queueUsed(false),
    m_needReinitDecoders(false),
    m_reverseMode(false),
    m_prevFrameToDelete(0),
    m_flushedBeforeReverseStart(false)
{
    for (int i = 0; i < MAX_FRAME_QUEUE_SIZE; ++i)
        m_frameQueue[i] = new CLVideoDecoderOutput();
}

CLVideoStreamDisplay::~CLVideoStreamDisplay()
{
    QMutexLocker _lock(&m_mtx);

    foreach(CLAbstractVideoDecoder* decoder, m_decoder)
    {
        delete decoder;
    }
    freeScaleContext();
    for (int i = 0; i < MAX_FRAME_QUEUE_SIZE; ++i)
        delete m_frameQueue[i];
    delete m_prevFrameToDelete;
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
    if (!m_scaleContext)
        cl_log.log(QLatin1String("Can't get swscale context"), cl_logERROR);
    return m_scaleContext != 0;
}

void CLVideoStreamDisplay::freeScaleContext()
{
	if (m_scaleContext) {
		sws_freeContext(m_scaleContext);
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

void CLVideoStreamDisplay::reorderPrevFrames()
{
    // find latest GOP (or several GOP. Decoder can add 1-3 (or even more) gops at once as 1 reverse block)
    for (int i = m_reverseQueue.size()-1; i >= 0; --i) 
    {
        if ((m_reverseQueue[i]->flags & AV_REVERSE_BLOCK_START) || i == 0) 
        {
            if (i == 0 && !(m_reverseQueue[i]->flags & AV_REVERSE_BLOCK_START)) {
                int ffd = 4;
            }
            std::reverse(m_reverseQueue.begin() + i, m_reverseQueue.end());
            for (int j = i; j < m_reverseQueue.size(); ++j) 
                m_reverseQueue[j]->flags |= AV_REVERSE_REORDERED;
            return;
        }
    }
}

bool CLVideoStreamDisplay::dispay(CLCompressedVideoData* data, bool draw, CLVideoDecoderOutput::downscale_factor force_factor)
{
    // use only 1 frame for non selected video
    bool enableFrameQueue = m_enableFrameQueue;
    if (!enableFrameQueue && m_queueUsed)
    {
        m_drawer->waitForFrameDisplayed(data->channelNumber);
        m_frameQueueIndex = 0;
        for (int i = 1; i < MAX_FRAME_QUEUE_SIZE; ++i)
            m_frameQueue[i]->clean();
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
			return true; // true to prevent 100% cpu usage on unknown codec
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
            return true;
        }
	}

    CLVideoDecoderOutput::downscale_factor scaleFactor = determineScaleFactor(data, dec->getWidth(), dec->getHeight(), force_factor);
    PixelFormat pixFmt = dec->GetPixelFormat();
    bool useTmpFrame =  !CLGLRenderer::isPixelFormatSupported(pixFmt) ||
        !CLVideoDecoderOutput::isPixelFormatSupported(pixFmt) || 
        scaleFactor > CLVideoDecoderOutput::factor_1;

    CLVideoDecoderOutput* outFrame = m_frameQueue[m_frameQueueIndex];
    if (!useTmpFrame)
        outFrame->setUseExternalData(!enableFrameQueue);

    if (data->flags & AV_REVERSE_BLOCK_START) 
    {
        CLCompressedVideoData emptyData(1,0);
        CLVideoDecoderOutput* tmpOutFrame = new CLVideoDecoderOutput();
        while (dec->decode(emptyData, tmpOutFrame)) {
            m_reverseQueue.enqueue(tmpOutFrame);
            tmpOutFrame = new CLVideoDecoderOutput();
        }
        delete tmpOutFrame;
        m_flushedBeforeReverseStart = true;
    }

	if (!dec || !dec->decode(*data, useTmpFrame ? &m_tmpFrame : outFrame))
	{
		//CL_LOG(cl_logDEBUG2) cl_log.log(QLatin1String("CLVideoStreamDisplay::dispay: decoder cannot decode image..."), cl_logDEBUG2);
        if (!m_reverseQueue.isEmpty() && (m_reverseQueue.front()->flags & AV_REVERSE_REORDERED)) {
            processDecodedFrame(data->channelNumber, m_reverseQueue.dequeue(), enableFrameQueue);
            return true;
        }
		return false;
	}

    if (m_flushedBeforeReverseStart) {
        data->flags |= AV_REVERSE_BLOCK_START;
        m_flushedBeforeReverseStart = false;
    }

    if (!data->context) {
        // for stiil images decoder parameters are not know before decoding, so recalculate parameters
        // It is got one more data copy from tmpFrame, but for movies we have got valid dst pointer (tmp or not) immediate before decoding
        // It is necessary for new logic with display queue
        scaleFactor = determineScaleFactor(data, dec->getWidth(), dec->getHeight(), force_factor);
        pixFmt = dec->GetPixelFormat();
    }

	if (!draw || !m_drawer)
		return true;

    if (useTmpFrame)
    {
        if (CLGLRenderer::isPixelFormatSupported(pixFmt) && CLVideoDecoderOutput::isPixelFormatSupported(pixFmt) && scaleFactor <= CLVideoDecoderOutput::factor_8)
            CLVideoDecoderOutput::downscale(&m_tmpFrame, outFrame, scaleFactor); // fast scaler
        else 
            rescaleFrame(m_tmpFrame, *outFrame, m_tmpFrame.width / scaleFactor, m_tmpFrame.height / scaleFactor); // universal scaler
    }
    outFrame->flags = data->flags;
    if (m_reverseMode) 
    {
        if (outFrame->flags & AV_REVERSE_BLOCK_START) 
            reorderPrevFrames();
        m_reverseQueue.enqueue(outFrame);

        m_frameQueue[m_frameQueueIndex] = new CLVideoDecoderOutput();
        if (!(m_reverseQueue.front()->flags & AV_REVERSE_REORDERED))
            return false; // frame does not ready. need more frames. does not perform wait
        outFrame = m_reverseQueue.dequeue();
    }
    
    processDecodedFrame(data->channelNumber, outFrame, enableFrameQueue);
    return true;
}

void CLVideoStreamDisplay::processDecodedFrame(int channel, CLVideoDecoderOutput* outFrame, bool enableFrameQueue)
{
    m_drawer->draw(outFrame, channel);

    if (enableFrameQueue) {
        m_frameQueueIndex = (m_frameQueueIndex + 1) % MAX_FRAME_QUEUE_SIZE; // allow frame queue for selected video
        m_queueUsed = true;
    }

    if (!enableFrameQueue) 
        m_drawer->waitForFrameDisplayed(channel);

    if (m_reverseMode) {
        if (m_prevFrameToDelete)
            delete m_prevFrameToDelete;
        m_prevFrameToDelete = outFrame;
    }
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

    if (outFrame.isExternalData() || outFrame.width != newWidth || outFrame.height != newHeight || outFrame.format != PIX_FMT_RGBA)
        outFrame.reallocate(newWidth, newHeight, PIX_FMT_RGBA);

    sws_scale(m_scaleContext,srcFrame.data, srcFrame.linesize, 0,
        srcFrame.height, outFrame.data, outFrame.linesize);
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

void CLVideoStreamDisplay::setReverseMode(bool value)
{
    if (value)
        m_enableFrameQueue = true;
    m_reverseMode = value;
}
