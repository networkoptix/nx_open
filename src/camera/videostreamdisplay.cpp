#include "videostreamdisplay.h"
#include "../base/log.h"
#include "../decoders/video/abstractdecoder.h"
#include "../data/mediadata.h"
#include "abstractrenderer.h"
#include "gl_renderer.h"
#include "util.h"
#include "base/longrunnable.h"
#include "base/adaptivesleep.h"

static const int MAX_REVERSE_QUEUE_SIZE = 1024*1024 * 300; // at bytes
static const double FPS_EPS = 1e-6;

class BufferedFrameDisplayer: public CLLongRunnable 
{
public:
    BufferedFrameDisplayer(CLAbstractRenderer* drawer): m_queue(MAX_FRAME_QUEUE_SIZE-1), m_drawer(drawer)
    {
        m_currentTime = AV_NOPTS_VALUE;
        m_expectedTime = AV_NOPTS_VALUE;
        m_lastDisplayedTime = AV_NOPTS_VALUE;
        start();
    }
    ~BufferedFrameDisplayer() {
        stop();
    }

    void waitForFramesDisplayed()
    {
        while (m_queue.size() > 0)
            msleep(1);
    }

    void addFrame(CLVideoDecoderOutput* outFrame) 
    {
        static const int MAX_QUEUE_TIME = 1000 * 200;
        bool needWait;
        do {
            m_sync.lock();
            needWait = !m_needStop && (m_queue.size() == m_queue.maxSize() || 
                        m_queue.size() > 0 && outFrame->pkt_dts - m_queue.front()->pkt_dts >= MAX_QUEUE_TIME);
            m_sync.unlock();
            if (needWait) 
                msleep(1);
        } while (needWait);
        m_queue.push(outFrame);
    }

    void setCurrentTime(qint64 time) {
        QMutexLocker lock(&m_sync);
        m_currentTime = time;
        m_timer.restart();
    }

    void clear() {
        stop();
        m_currentTime = m_expectedTime = AV_NOPTS_VALUE;
        start();
    }

    qint64 getLastDisplayedTime() 
    {
        QMutexLocker lock(&m_sync);
        return m_lastDisplayedTime;
    }

    void setLastDisplayedTime(qint64 value)
    {
        QMutexLocker lock(&m_sync);
        m_lastDisplayedTime = value;
    }

protected:
    virtual void run() 
    {
        CLVideoDecoderOutput* frame;
        while (!m_needStop)
        {
            if (m_queue.size() > 0)
            {
                frame = m_queue.front();

                if (m_expectedTime == AV_NOPTS_VALUE) {
                    m_alignedTimer.restart();
                    m_expectedTime = frame->pkt_dts;
                }

                m_sync.lock();
                qint64 expectedTime = m_expectedTime  + m_alignedTimer.elapsed()*1000ll;
                qint64 currentTime = m_currentTime != AV_NOPTS_VALUE ? m_currentTime + m_timer.elapsed()*1000ll : expectedTime;
                
                // align to grid
                if (qAbs(expectedTime - currentTime) < 60000) {
                    currentTime = expectedTime;
                }
                else {
                    m_alignedTimer.restart();
                    m_expectedTime = currentTime;
                }
                if (frame->pkt_dts - currentTime >  MAX_VALID_SLEEP_TIME) {
                    currentTime = m_currentTime = m_expectedTime = frame->pkt_dts;
                    m_alignedTimer.restart();
                    m_timer.restart();
                }
                qint64 sleepTime = frame->pkt_dts - currentTime;

                m_sync.unlock();

                if (sleepTime > 0)
                    msleep(sleepTime/1000);
                else if (sleepTime < -2000000) {
                    m_currentTime = m_expectedTime = AV_NOPTS_VALUE;
                }

                m_sync.lock();
                m_lastDisplayedTime = frame->pkt_dts;
                m_sync.unlock();

                m_drawer->draw(frame);
                //m_drawer->waitForFrameDisplayed(0);
                m_sync.lock();
                m_queue.pop(frame);
                m_sync.unlock();
            }
            else {
                msleep(1);
            }
        }
        while (m_queue.size() > 0)
            m_queue.pop(frame);
    }
private:
    qint64 m_expectedTime;
    CLNonReferredThreadQueue<CLVideoDecoderOutput*> m_queue;
    QTime m_timer;
    QTime m_alignedTimer;
    CLAbstractRenderer* m_drawer;
    qint64 m_currentTime;
    QMutex m_sync;
    qint64 m_lastDisplayedTime;
};

// ------------------- CLVideoStreamDisplay --------------------------

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
    m_prevReverseMode(false),
    m_prevFrameToDelete(0),
    m_flushedBeforeReverseStart(false),
    m_lastDisplayedTime(0),
    m_realReverseSize(0),
    m_maxReverseQueueSize(-1),
    m_timeChangeEnabled(true),
    m_bufferedFrameDisplayer(0),
    m_speed(1.0)
{
    for (int i = 0; i < MAX_FRAME_QUEUE_SIZE; ++i)
        m_frameQueue[i] = new CLVideoDecoderOutput();
}

CLVideoStreamDisplay::~CLVideoStreamDisplay()
{
    delete m_bufferedFrameDisplayer;
    QMutexLocker _lock(&m_mtx);

    foreach(CLAbstractVideoDecoder* decoder, m_decoder)
    {
        delete decoder;
    }
    freeScaleContext();
    for (int i = 0; i < MAX_FRAME_QUEUE_SIZE; ++i)
        delete m_frameQueue[i];
    for (int i = 0; i < m_reverseQueue.size(); ++i)
        delete m_reverseQueue[i];
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
    if (m_reverseQueue.isEmpty())
        return;
    // find latest GOP (or several GOP. Decoder can add 1-3 (or even more) gops at once as 1 reverse block)
    int i = m_reverseQueue.size()-1;
    for (; i >= 0; --i) 
    {
        if (m_reverseQueue[i]->flags & AV_REVERSE_REORDERED)
            break;
        if ((m_reverseQueue[i]->flags & AV_REVERSE_BLOCK_START) || i == 0) 
        {
            std::reverse(m_reverseQueue.begin() + i, m_reverseQueue.end());
            for (int j = i; j < m_reverseQueue.size(); ++j) 
                m_reverseQueue[j]->flags |= AV_REVERSE_REORDERED;
            return;
        }
    }
    i++;
    if (i < m_reverseQueue.size()) {
        std::reverse(m_reverseQueue.begin() + i, m_reverseQueue.end());
        for (int j = i; j < m_reverseQueue.size(); ++j) 
            m_reverseQueue[j]->flags |= AV_REVERSE_REORDERED;
    }
}

void CLVideoStreamDisplay::checkQueueOverflow(CLAbstractVideoDecoder* dec)
{
    if (m_maxReverseQueueSize == -1) {
        int bytesPerPic = avpicture_get_size(dec->getFormat(), dec->getWidth(), dec->getHeight());
        m_maxReverseQueueSize = MAX_REVERSE_QUEUE_SIZE / bytesPerPic;
    }
    
    if (m_realReverseSize <= m_maxReverseQueueSize)
        return;
    // drop some frame at queue. Find max interval contains non-dropped frames (and drop frame from mid of this interval)
    int maxInterval = -1;
    int prevIndex = -1;
    int maxStart = 0;

    for (int i = 0; i < m_reverseQueue.size(); ++i) 
    {
        if (m_reverseQueue[i]->data[0])
        {
            int start = i;
            for(; i < m_reverseQueue.size() && m_reverseQueue[i]->data[0]; ++i);

            if (i - start > maxInterval) {
                maxInterval = i - start;
                maxStart = start;
            }
        }
    }
    int index;
    if (maxInterval == 1) 
    {
        // every 2-nd frame already dropped. Change strategy. Increase min hole interval by 1
        int minHole = INT_MAX;
        for (int i = m_reverseQueue.size()-1; i >= 0; --i) {
            if (!m_reverseQueue[i]->data[0])
            {
                int start = i;
                for(; i >= 0 && !m_reverseQueue[i]->data[0]; --i);

                if (start - i < minHole) {
                    minHole = start - i;
                    maxStart = i+1;
                }
            }
        }
        if (maxStart + minHole < m_reverseQueue.size())
            index = maxStart + minHole; // take right frame from the hole
        else
            index = maxStart-1; // take left frame
    }
    else {
        index = maxStart + maxInterval/2;
    }
    Q_ASSERT(m_reverseQueue[index]->data[0]);
    m_reverseQueue[index]->reallocate(0,0,0);
    m_realReverseSize--;
}

void CLVideoStreamDisplay::waitForFramesDisplaed()
{
    if (m_bufferedFrameDisplayer)
        m_bufferedFrameDisplayer->waitForFramesDisplayed();
}

CLVideoStreamDisplay::FrameDisplayStatus CLVideoStreamDisplay::dispay(CLCompressedVideoData* data, bool draw, CLVideoDecoderOutput::downscale_factor force_factor)
{
    m_mtx.lock();
    // use only 1 frame for non selected video
    bool reverseMode = m_reverseMode;

    bool enableFrameQueue = reverseMode ? true : m_enableFrameQueue;
    if (enableFrameQueue && qAbs(m_speed - 1.0) < FPS_EPS) 
    {
        if (!m_bufferedFrameDisplayer) {
            QMutexLocker lock(&m_timeMutex);
            m_bufferedFrameDisplayer = new BufferedFrameDisplayer(m_drawer);
        }
    }
    else 
    {
        if (m_bufferedFrameDisplayer) 
        {
            m_bufferedFrameDisplayer->waitForFramesDisplayed();
            setLastDisplayedTime(m_bufferedFrameDisplayer->getLastDisplayedTime());
            QMutexLocker lock(&m_timeMutex);
            delete m_bufferedFrameDisplayer;
            m_bufferedFrameDisplayer = 0;
        }
    }

    if (!enableFrameQueue && m_queueUsed)
    {
        m_drawer->waitForFrameDisplayed(data->channelNumber);
        m_frameQueueIndex = 0;
        for (int i = 1; i < MAX_FRAME_QUEUE_SIZE; ++i)
        {
            if (!m_frameQueue[i]->isExternalData())
                m_frameQueue[i]->clean();
        }
        m_queueUsed = false;
    }
    //if (!reverseMode && m_reverseQueue.size() > 0) 
    //    clearReverseQueue();

    if (m_needReinitDecoders) {
        foreach(CLAbstractVideoDecoder* decoder, m_decoder)
            decoder->setMTDecoding(enableFrameQueue);
        m_needReinitDecoders = false;
    }

    CLVideoDecoderOutput m_tmpFrame;
    m_tmpFrame.setUseExternalData(true);

	if (data->compressionType == CODEC_ID_NONE)
	{
		cl_log.log(QLatin1String("CLVideoStreamDisplay::dispay: unknown codec type..."), cl_logERROR);
        m_mtx.unlock();
        return Status_Displayed; // true to prevent 100% cpu usage on unknown codec
	}

    CLAbstractVideoDecoder* dec = m_decoder[data->compressionType];
	if (dec == 0)
	{
		dec = CLVideoDecoderFactory::createDecoder(data->compressionType, data->context);
		dec->setLightCpuMode(m_lightCPUmode);
        m_decoder.insert(data->compressionType, dec);
	}
    if (dec == 0) {
        CL_LOG(cl_logDEBUG2) cl_log.log(QLatin1String("Can't find video decoder"), cl_logDEBUG2);
        m_mtx.unlock();
        return Status_Displayed;
    }

    if (reverseMode != m_prevReverseMode) {
        clearReverseQueue();
        dec->resetDecoder();
        m_prevReverseMode = reverseMode;
    }

    CLVideoDecoderOutput::downscale_factor scaleFactor = determineScaleFactor(data, dec->getWidth(), dec->getHeight(), force_factor);
    PixelFormat pixFmt = dec->GetPixelFormat();
    bool useTmpFrame =  !CLGLRenderer::isPixelFormatSupported(pixFmt) ||
        !CLVideoDecoderOutput::isPixelFormatSupported(pixFmt) || 
        scaleFactor > CLVideoDecoderOutput::factor_1;

    CLVideoDecoderOutput* outFrame = m_frameQueue[m_frameQueueIndex];
    outFrame->channel = data->channelNumber;

    if (outFrame->isDisplaying()) 
        m_drawer->waitForFrameDisplayed(data->channelNumber);
    
    if (!useTmpFrame)
        outFrame->setUseExternalData(!enableFrameQueue);

    if (data->flags & CLAbstractMediaData::MediaFlags_AfterEOF)
        dec->resetDecoder();

    if ((data->flags & AV_REVERSE_BLOCK_START) && m_lightCPUmode != CLAbstractVideoDecoder::DecodeMode_Fastest)
    {
        CLCompressedVideoData emptyData(1,0);
        CLVideoDecoderOutput* tmpOutFrame = new CLVideoDecoderOutput();
        while (dec->decode(emptyData, tmpOutFrame)) 
        {
            {
                tmpOutFrame->pkt_dts = AV_NOPTS_VALUE;
                m_reverseQueue.enqueue(tmpOutFrame);
                m_realReverseSize++;
                checkQueueOverflow(dec);
                tmpOutFrame = new CLVideoDecoderOutput();
            }
        }
        delete tmpOutFrame;
        m_flushedBeforeReverseStart = true;
        reorderPrevFrames();
        dec->resetDecoder();
    }

    CLVideoDecoderOutput* decodeToFrame = useTmpFrame ? &m_tmpFrame : outFrame;
	if (!dec || !dec->decode(*data, decodeToFrame))
	{
        m_mtx.unlock();
        if (m_lightCPUmode == CLAbstractVideoDecoder::DecodeMode_Fastest)
            return Status_Skipped;
        if (!m_reverseQueue.isEmpty() && (m_reverseQueue.front()->flags & AV_REVERSE_REORDERED)) {
            outFrame = m_reverseQueue.dequeue();
            if (outFrame->data[0])
                m_realReverseSize--;

            outFrame->sample_aspect_ratio = dec->getSampleAspectRatio();
            if (processDecodedFrame(outFrame, enableFrameQueue, reverseMode))
                return Status_Displayed;
            else
                return Status_Buffered;
        }
		return Status_Skipped;
	}
    m_mtx.unlock();

    if (qAbs(decodeToFrame->pkt_dts-data->timestamp) > 200*1000) {
        // prevent large difference after seek or EOF
        outFrame->pkt_dts = data->timestamp;
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
		return Status_Displayed;

    if (useTmpFrame)
    {
        if (CLGLRenderer::isPixelFormatSupported(pixFmt) && CLVideoDecoderOutput::isPixelFormatSupported(pixFmt) && scaleFactor <= CLVideoDecoderOutput::factor_8)
            CLVideoDecoderOutput::downscale(&m_tmpFrame, outFrame, scaleFactor); // fast scaler
        else {
            if (!rescaleFrame(m_tmpFrame, *outFrame, m_tmpFrame.width / scaleFactor, m_tmpFrame.height / scaleFactor)) // universal scaler
                return Status_Displayed;
        }
        outFrame->pkt_dts = m_tmpFrame.pkt_dts;
    }
    outFrame->flags = data->flags;
    //outFrame->pts = data->timestamp;
    if (reverseMode) 
    {
        if (outFrame->flags & AV_REVERSE_BLOCK_START) {
            reorderPrevFrames();
        }
        m_reverseQueue.enqueue(outFrame);
        m_realReverseSize++;
        checkQueueOverflow(dec);
        m_frameQueue[m_frameQueueIndex] = new CLVideoDecoderOutput();
        if (!(m_reverseQueue.front()->flags & AV_REVERSE_REORDERED))
            return Status_Buffered; // frame does not ready. need more frames. does not perform wait
        outFrame = m_reverseQueue.dequeue();
        if (outFrame->data[0])
            m_realReverseSize--;
    }
    
    outFrame->sample_aspect_ratio = dec->getSampleAspectRatio();

    if (processDecodedFrame(outFrame, enableFrameQueue, reverseMode))
        return Status_Displayed;
    else
        return Status_Buffered;
}

bool CLVideoStreamDisplay::processDecodedFrame(CLVideoDecoderOutput* outFrame, bool enableFrameQueue, bool reverseMode)
{
    if (outFrame->pkt_dts != AV_NOPTS_VALUE)
        setLastDisplayedTime(outFrame->pkt_dts);
    if (outFrame->data[0]) {
        if (enableFrameQueue) 
        {
            if (m_bufferedFrameDisplayer)
                m_bufferedFrameDisplayer->addFrame(outFrame);
            else
                m_drawer->draw(outFrame);
            m_frameQueueIndex = (m_frameQueueIndex + 1) % MAX_FRAME_QUEUE_SIZE; // allow frame queue for selected video
            m_queueUsed = true;
        }
        else {
            m_drawer->draw(outFrame);
            m_drawer->waitForFrameDisplayed(outFrame->channel);
        }
        if (reverseMode) {
            if (m_prevFrameToDelete)
                delete m_prevFrameToDelete;
            m_prevFrameToDelete = outFrame;
        }
        return !m_bufferedFrameDisplayer;
    }
    else {
        delete outFrame;
        return false;
    }
}

bool CLVideoStreamDisplay::rescaleFrame(const CLVideoDecoderOutput& srcFrame, CLVideoDecoderOutput& outFrame, int newWidth, int newHeight)
{
    static const int ROUND_FACTOR = 16;
    // due to openGL requirements chroma MUST be devided by 4, luma MUST be devided by 8
    // due to MMX scaled functions requirements chroma MUST be devided by 8, so luma MUST be devided by 16
    newWidth = roundUp(newWidth, ROUND_FACTOR);

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

void CLVideoStreamDisplay::setSpeed(float value)
{
    m_speed = value;
    m_reverseMode = value < 0;
    if (m_reverseMode)
        m_enableFrameQueue = true;
}

qint64 CLVideoStreamDisplay::getLastDisplayedTime() const 
{ 
    QMutexLocker lock(&m_timeMutex);
    if (m_bufferedFrameDisplayer && m_timeChangeEnabled)
        return m_bufferedFrameDisplayer->getLastDisplayedTime();
    else
        return m_lastDisplayedTime; 
}

void CLVideoStreamDisplay::setLastDisplayedTime(qint64 value) 
{ 
    QMutexLocker lock(&m_timeMutex);
    if (m_timeChangeEnabled)
        m_lastDisplayedTime = value; 
}

void CLVideoStreamDisplay::blockTimeValue(qint64 time)
{
    QMutexLocker lock(&m_timeMutex);
    m_lastDisplayedTime = time;
    if (m_bufferedFrameDisplayer)
        m_bufferedFrameDisplayer->setLastDisplayedTime(time);
    m_timeChangeEnabled = false;
}

void CLVideoStreamDisplay::unblockTimeValue()
{
    QMutexLocker lock(&m_timeMutex);
    if (m_bufferedFrameDisplayer)
        m_bufferedFrameDisplayer->setLastDisplayedTime(m_lastDisplayedTime);
    m_timeChangeEnabled = true;
    cl_log.log("unblock time value", cl_logALWAYS);
}

void CLVideoStreamDisplay::afterJump()
{
    clearReverseQueue();
    if (m_bufferedFrameDisplayer)
        m_bufferedFrameDisplayer->clear();
    //for (QMap<CodecID, CLAbstractVideoDecoder*>::iterator itr = m_decoder.begin(); itr != m_decoder.end(); ++itr)
    //    (*itr)->resetDecoder();
}

void CLVideoStreamDisplay::clearReverseQueue()
{
    m_drawer->waitForFrameDisplayed(0);
    for (int i = 0; i < m_reverseQueue.size(); ++i)
        delete m_reverseQueue[i];
    m_reverseQueue.clear();
    m_realReverseSize = 0;
}

QImage CLVideoStreamDisplay::getScreenshot()
{
    if (m_decoder.isEmpty())
        return QImage();
    CLAbstractVideoDecoder* dec = m_decoder.begin().value();
    QMutexLocker mutex(&m_mtx);
    const AVFrame* lastFrame = dec->lastFrame();
    AVFrame deinterlacedFrame;

    // deinterlace if need
    if (lastFrame->interlaced_frame)
    {
        int numBytes = avpicture_get_size(dec->getFormat(), dec->getWidth(), dec->getHeight());
        avpicture_fill((AVPicture*)&deinterlacedFrame, (quint8*) av_malloc(numBytes), dec->getFormat(), dec->getWidth(), dec->getHeight());
        if (avpicture_deinterlace((AVPicture*)&deinterlacedFrame, (AVPicture*) lastFrame, dec->getFormat(), dec->getWidth(), dec->getHeight()) == 0)
            lastFrame = &deinterlacedFrame;
    }

    // convert colorSpace
    SwsContext *convertor;
    convertor = sws_getContext(dec->getWidth(), dec->getHeight(), dec->getFormat(),
        dec->getWidth(), dec->getHeight(), PIX_FMT_BGRA,
        SWS_POINT, NULL, NULL, NULL);
    if (!convertor) {
        if (lastFrame->interlaced_frame)
            av_free(deinterlacedFrame.data[0]);
        return QImage();
    }

    int numBytes = avpicture_get_size(PIX_FMT_RGBA, dec->getWidth(), dec->getHeight());
    AVPicture outPicture; 
    avpicture_fill( (AVPicture*) &outPicture, (quint8*) av_malloc(numBytes), PIX_FMT_BGRA, dec->getWidth(), dec->getHeight());

    sws_scale(convertor, lastFrame->data, lastFrame->linesize, 
              0, dec->getHeight(), 
              outPicture.data, outPicture.linesize);
    sws_freeContext(convertor);
    // convert to QImage
    QImage tmp(outPicture.data[0], dec->getWidth(), dec->getHeight(), outPicture.linesize[0], QImage::Format_ARGB32);
    QImage rez( dec->getWidth(), dec->getHeight(), QImage::Format_ARGB32);
    rez = tmp.copy(0,0, dec->getWidth(), dec->getHeight());
    avpicture_free(&outPicture);
    if (lastFrame->interlaced_frame)
        av_free(deinterlacedFrame.data[0]);
    return rez;
}

void CLVideoStreamDisplay::setCurrentTime(qint64 time)
{
    if (m_bufferedFrameDisplayer)
        m_bufferedFrameDisplayer->setCurrentTime(time);
}
