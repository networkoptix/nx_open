
#include "video_stream_display.h"

#include <algorithm>

#include <client/client_settings.h>

#include "decoders/video/abstract_video_decoder.h"
#include "utils/math/math.h"
#include "nx/utils/thread/long_runnable.h"
#include "utils/common/adaptive_sleep.h"
#include <nx/utils/log/log.h>

#include "abstract_renderer.h"
#include "gl_renderer.h"
#include "buffered_frame_displayer.h"
#include "ui/graphics/opengl/gl_functions.h"
#include "ui/graphics/items/resource/resource_widget_renderer.h"
#include <camera/video_decoder_factory.h>

static const int MAX_REVERSE_QUEUE_SIZE = 1024*1024 * 300; // at bytes
static const double FPS_EPS = 1e-6;

QnVideoStreamDisplay::QnVideoStreamDisplay(bool canDownscale, int channelNumber) :
    m_frameQueueIndex(0),
    m_decodeMode(QnAbstractVideoDecoder::DecodeMode_Full),
    m_canDownscale(canDownscale),
    m_channelNumber(channelNumber),
    m_prevFactor(QnFrameScaler::factor_1),
    m_scaleFactor(QnFrameScaler::factor_1),
    m_previousOnScreenSize(0, 0),
    m_scaleContext(NULL),
    m_outputWidth(0),
    m_outputHeight(0),
    m_enableFrameQueue(false),
    m_queueUsed(false),
    m_needReinitDecoders(false),
    m_reverseMode(false),
    m_prevReverseMode(false),
    m_flushedBeforeReverseStart(false),
    m_reverseSizeInBytes(0),
    m_timeChangeEnabled(true),
    m_bufferedFrameDisplayer(0),
    m_canUseBufferedFrameDisplayer(true),
    m_rawDataSize(0,0),
    m_speed(1.0),
    m_queueWasFilled(false),
    m_needResetDecoder(false),
    m_lastDisplayedFrame(NULL),
    m_prevSrcWidth(0),
    m_prevSrcHeight(0),
    m_lastIgnoreTime(AV_NOPTS_VALUE),
    m_isPaused(false),
    m_overridenAspectRatio(0.0)
{
    for (int i = 0; i < kMaxFrameQueueSize; ++i)
        m_frameQueue[i] = QSharedPointer<CLVideoDecoderOutput>( new CLVideoDecoderOutput() );
}

QnVideoStreamDisplay::~QnVideoStreamDisplay()
{
    foreach(QnAbstractRenderer* renderer, m_renderList)
        renderer->notInUse();

    m_renderList = m_newList;
    m_renderListModified = false;


    delete m_bufferedFrameDisplayer;
    QnMutexLocker _lock( &m_mtx );

    foreach(QnAbstractVideoDecoder* decoder, m_decoder)
    {
        delete decoder;
    }
    freeScaleContext();
}

void QnVideoStreamDisplay::pleaseStop()
{
    //foreach(QnAbstractRenderer* render, m_renderList)
    //    render->pleaseStop();
}

int QnVideoStreamDisplay::addRenderer(QnAbstractRenderer* renderer)
{
    {
        QnMutexLocker lock( &m_mtx );
        renderer->setPaused(m_isPaused);
        if (m_lastDisplayedFrame)
            renderer->draw(m_lastDisplayedFrame);
    }

    QnMutexLocker lock( &m_renderListMtx );
    renderer->setScreenshotInterface(this);
    m_newList << renderer;
    m_renderListModified = true;
    return m_newList.size();
}

int QnVideoStreamDisplay::removeRenderer(QnAbstractRenderer* renderer)
{
    QnMutexLocker lock( &m_renderListMtx );
    m_newList.remove(renderer);
    m_renderListModified = true;
    return m_newList.size();
}

void QnVideoStreamDisplay::addMetadataConsumer(
    const nx::media::AbstractMetadataConsumerPtr& metadataConsumer)
{
    QnMutexLocker lock(&m_metadataConsumersHashMutex);
    m_metadataConsumerByType.insert(metadataConsumer->metadataType(), metadataConsumer);
}

void QnVideoStreamDisplay::removeMetadataConsumer(
    const nx::media::AbstractMetadataConsumerPtr& metadataConsumer)
{
    QnMutexLocker lock(&m_metadataConsumersHashMutex);
    m_metadataConsumerByType.remove(metadataConsumer->metadataType(), metadataConsumer);
}

QnFrameScaler::DownscaleFactor QnVideoStreamDisplay::getCurrentDownscaleFactor() const
{
    return m_scaleFactor;
}

bool QnVideoStreamDisplay::allocScaleContext( const CLVideoDecoderOutput& outFrame, int newWidth, int newHeight )
{
    m_outputWidth = newWidth;
    m_outputHeight = newHeight;
    m_scaleContext = sws_getContext(outFrame.width, outFrame.height, (AVPixelFormat) outFrame.format,
                                    m_outputWidth, m_outputHeight, AV_PIX_FMT_RGBA,
                                    SWS_POINT, NULL, NULL, NULL);
    if (!m_scaleContext)
        NX_ERROR(this, "Can't get swscale context");
    return m_scaleContext != 0;
}

void QnVideoStreamDisplay::freeScaleContext()
{
    if (m_scaleContext) {
        sws_freeContext(m_scaleContext);
        m_scaleContext = 0;
    }
}

QnFrameScaler::DownscaleFactor QnVideoStreamDisplay::determineScaleFactor(QSet<QnAbstractRenderer*> renderList,
                                                                          int channelNumber,
                                                                          int srcWidth, int srcHeight,
                                                                          QnFrameScaler::DownscaleFactor force_factor)
{
    for (QnAbstractRenderer* render: renderList) {
        if (render->constantDownscaleFactor())
            force_factor = QnFrameScaler::factor_1;
    }

    if (force_factor==QnFrameScaler::factor_any) // if nobody pushing lets peek it
    {
        QSize on_screen;
        for (QnAbstractRenderer* render: renderList) {
            QSize size = render->sizeOnScreen(channelNumber);
            on_screen.setWidth(qMax(on_screen.width(), size.width()));
            on_screen.setHeight(qMax(on_screen.height(), size.height()));
        }
        /* Check if item size is not calculated yet. */
        if (on_screen.isEmpty()) {
            on_screen.setHeight(srcHeight);
            on_screen.setWidth(srcWidth);
        }

        m_scaleFactor = findScaleFactor(srcWidth, srcHeight, on_screen.width(), on_screen.height());

        if (m_scaleFactor < m_prevFactor && m_prevSrcWidth == srcWidth && m_prevSrcHeight == srcHeight)
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
        m_prevSrcWidth = srcWidth;
        m_prevSrcHeight = srcHeight;

        if (m_scaleFactor != m_prevFactor)
        {
            m_previousOnScreenSize = on_screen;
            m_prevFactor = m_scaleFactor;
        }
    }
    else
        m_scaleFactor = force_factor;

    QnFrameScaler::DownscaleFactor rez = m_canDownscale ? qMax(m_scaleFactor, QnFrameScaler::factor_1) : QnFrameScaler::factor_1;
    // If there is no scaling needed check if size is greater than maximum allowed image size (maximum texture size for opengl).
    int newWidth = srcWidth / rez;
    int newHeight = srcHeight / rez;
    int maxTextureSize = QnGlFunctions::estimatedInteger(GL_MAX_TEXTURE_SIZE);
    while ((maxTextureSize > 0 && newWidth > maxTextureSize) || newHeight > maxTextureSize)
    {
        rez = QnFrameScaler::DownscaleFactor ((int)rez * 2);
        newWidth /= 2;
        newHeight /= 2;
    }
    return rez;
}

void QnVideoStreamDisplay::reorderPrevFrames()
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

void QnVideoStreamDisplay::checkQueueOverflow(QnAbstractVideoDecoder* dec)
{
    Q_UNUSED(dec)
    while (m_reverseSizeInBytes > MAX_REVERSE_QUEUE_SIZE)
    {
        // drop some frame at queue. Find max interval contains non-dropped frames (and drop frame from mid of this interval)
        int maxInterval = -1;
        //int prevIndex = -1;
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
        NX_ASSERT( m_reverseQueue[index]->data[0] || m_reverseQueue[index]->picData );
        m_reverseSizeInBytes -= avpicture_get_size((AVPixelFormat) m_reverseQueue[index]->format, m_reverseQueue[index]->width, m_reverseQueue[index]->height);
        m_reverseQueue[index]->reallocate(0,0,0);
    }
}

void QnVideoStreamDisplay::waitForFramesDisplayed()
{
    if (m_bufferedFrameDisplayer) {
        m_bufferedFrameDisplayer->waitForFramesDisplayed();
    }
    else {
        foreach(QnAbstractRenderer* renderer, m_renderList)
            renderer->waitForFrameDisplayed(0); // wait old frame
    }
    m_queueWasFilled = false;
}

qint64 QnVideoStreamDisplay::nextReverseTime() const
{
    for (int i = 0; i < m_reverseQueue.size(); ++i)
    {
        if (m_reverseQueue[i]->pkt_dts != AV_NOPTS_VALUE)
        {
            if (m_reverseQueue[i]->flags & AV_REVERSE_REORDERED)
                return m_reverseQueue[i]->pkt_dts;
            else
                return AV_NOPTS_VALUE;
        }
    }
    return AV_NOPTS_VALUE;
}


QSharedPointer<CLVideoDecoderOutput> QnVideoStreamDisplay::flush(QnFrameScaler::DownscaleFactor force_factor, int channelNum)
{
    foreach(QnAbstractRenderer* render, m_renderList)
        render->finishPostedFramesRender(channelNum);

    QSharedPointer<CLVideoDecoderOutput> tmpFrame(new CLVideoDecoderOutput());
    tmpFrame->setUseExternalData(false);


    if (m_decoder.isEmpty())
        return QSharedPointer<CLVideoDecoderOutput>();
    QnAbstractVideoDecoder* dec = m_decoder.begin().value();
    if (dec == 0)
        return QSharedPointer<CLVideoDecoderOutput>();

    QnFrameScaler::DownscaleFactor scaleFactor = determineScaleFactor(m_renderList, channelNum, dec->getWidth(), dec->getHeight(), force_factor);

    QSharedPointer<CLVideoDecoderOutput> outFrame = m_frameQueue[m_frameQueueIndex];
    outFrame->channel = channelNum;

    foreach(QnAbstractRenderer* render, m_renderList)
    {
        if (render->isDisplaying(outFrame))
            render->finishPostedFramesRender(channelNum);
    }

    outFrame->channel = channelNum;

    m_mtx.lock();

    QnWritableCompressedVideoDataPtr emptyData(new QnWritableCompressedVideoData(1,0));
    while (dec->decode(emptyData, &tmpFrame))
    {
        calcSampleAR(outFrame, dec);

        if( !(dec->getDecoderCaps() & QnAbstractVideoDecoder::decodedPictureScaling) )
        {
            AVPixelFormat pixFmt = dec->GetPixelFormat();
            if (!downscaleFrame(tmpFrame, outFrame, scaleFactor, pixFmt))
                continue;
        }
        foreach(QnAbstractRenderer* render, m_renderList)
            render->draw(outFrame);
        foreach(QnAbstractRenderer* render, m_renderList)
            render->finishPostedFramesRender(channelNum);
    }

    if (tmpFrame->width == 0) {
        getLastDecodedFrame( dec, &tmpFrame );
    }

    m_mtx.unlock();

    return tmpFrame;
}

void QnVideoStreamDisplay::updateRenderList()
{
    QnMutexLocker lock( &m_renderListMtx );
    if (m_renderListModified)
    {
        foreach(QnAbstractRenderer* renderer, m_newList)
            renderer->inUse();

        foreach(QnAbstractRenderer* renderer, m_renderList)
            renderer->notInUse();

        m_renderList = m_newList;
        if (m_bufferedFrameDisplayer)
            m_bufferedFrameDisplayer->setRenderList(m_renderList);
        m_renderListModified = false;
    }
};

void QnVideoStreamDisplay::calcSampleAR(QSharedPointer<CLVideoDecoderOutput> outFrame, QnAbstractVideoDecoder* dec)
{
    if (qFuzzyIsNull(m_overridenAspectRatio))
    {
        outFrame->sample_aspect_ratio = dec->getSampleAspectRatio();
    }
    else {
        qreal realAR = outFrame->height > 0 ? (qreal) outFrame->width / (qreal) outFrame->height : 1.0;
        outFrame->sample_aspect_ratio = m_overridenAspectRatio / realAR;
    }
}

QnVideoStreamDisplay::FrameDisplayStatus QnVideoStreamDisplay::display(QnCompressedVideoDataPtr data, bool draw, QnFrameScaler::DownscaleFactor force_factor)
{
    updateRenderList();

    // use only 1 frame for non selected video
    const bool reverseMode = m_reverseMode;

    if (reverseMode)
        m_lastIgnoreTime = AV_NOPTS_VALUE;
    else if (!draw)
        m_lastIgnoreTime = data->timestamp;

    bool enableFrameQueue;
    bool needReinitDecoders;
    {
        QnMutexLocker lock(&m_mtx);
        enableFrameQueue = reverseMode ? true : m_enableFrameQueue;
        needReinitDecoders = m_needReinitDecoders;
        m_needReinitDecoders = false;
    }

    if (enableFrameQueue && qAbs(m_speed - 1.0) < FPS_EPS && m_canUseBufferedFrameDisplayer)
    {
        if (!m_bufferedFrameDisplayer) {
            //QnMutexLocker lock( &m_timeMutex );
            m_bufferedFrameDisplayer = new QnBufferedFrameDisplayer();
            m_bufferedFrameDisplayer->setRenderList(m_renderList);
            m_queueWasFilled = false;
        }
    }
    else
    {
        if (m_bufferedFrameDisplayer)
        {
            m_bufferedFrameDisplayer->waitForFramesDisplayed();
            //overrideTimestampOfNextFrameToRender(m_bufferedFrameDisplayer->getTimestampOfNextFrameToRender());
            //QnMutexLocker lock( &m_timeMutex );
            delete m_bufferedFrameDisplayer;
            m_bufferedFrameDisplayer = 0;
        }
    }

    if (!enableFrameQueue && m_queueUsed)
    {
        foreach(QnAbstractRenderer* render, m_renderList)
            render->waitForFrameDisplayed(data->channelNumber);
        m_frameQueueIndex = 0;
        for (int i = 1; i < kMaxFrameQueueSize; ++i)
        {
            if (!m_frameQueue[i]->isExternalData())
                m_frameQueue[i]->clean();
        }
        m_queueUsed = false;
    }

    if (needReinitDecoders) {
        QnMutexLocker lock( &m_mtx );
        foreach(QnAbstractVideoDecoder* decoder, m_decoder)
            decoder->setMTDecoding(enableFrameQueue);
    }

    QSharedPointer<CLVideoDecoderOutput> m_tmpFrame( new CLVideoDecoderOutput() );
    m_tmpFrame->setUseExternalData(true);   //temp frame will take internal ffmpeg buffer for optimization

    if (data->compressionType == AV_CODEC_ID_NONE)
    {
        NX_ERROR(this, "display: unknown codec type...");
        return Status_Displayed; // true to prevent 100% cpu usage on unknown codec
    }

    QnAbstractVideoDecoder* dec = m_decoder[data->compressionType];
    if (dec == 0)
    {
        // todo: all renders MUST have same GL context!!!
        const QnAbstractRenderer* renderer = m_renderList.isEmpty() ? 0 : *m_renderList.constBegin();
        const QnResourceWidgetRenderer* widgetRenderer = dynamic_cast<const QnResourceWidgetRenderer*>(renderer);
        dec = QnVideoDecoderFactory::createDecoder(
                data,
                enableFrameQueue,
                widgetRenderer ? widgetRenderer->glContext() : NULL);
        dec->setSpeed( m_speed );
        if (dec == 0)
        {
            NX_VERBOSE(this, lit("Can't find create decoder for compression type %1").arg(data->compressionType));
            return Status_Displayed;
        }

        dec->setLightCpuMode(m_decodeMode);
        m_decoder.insert(data->compressionType, dec);
    }

    if (reverseMode != m_prevReverseMode || m_needResetDecoder)
    {
        clearReverseQueue();
        QnMutexLocker lock( &m_mtx );
        dec->resetDecoder(data);
        m_prevReverseMode = reverseMode;
        m_needResetDecoder = false;
        //data->flags |= QnAbstractMediaData::MediaFlags_DecodeTwice;
    }

    dec->setOutPictureSize(getMaxScreenSize());

    QnFrameScaler::DownscaleFactor scaleFactor = QnFrameScaler::factor_unknown;
    if (dec->getWidth() > 0)
    {
        scaleFactor = determineScaleFactor(m_renderList, data->channelNumber, dec->getWidth(), dec->getHeight(), force_factor);
    }

    AVPixelFormat pixFmt = dec->GetPixelFormat();

    //if true, decoding to tmp frame which will be later scaled/converted to supported format
    const bool useTmpFrame =
        (dec->targetMemoryType() == QnAbstractPictureDataRef::pstSysMemPic) &&
        (!QnGLRenderer::isPixelFormatSupported(pixFmt) ||
         !CLVideoDecoderOutput::isPixelFormatSupported(pixFmt) ||
         scaleFactor != QnFrameScaler::factor_1);

    QSharedPointer<CLVideoDecoderOutput> outFrame = m_frameQueue[m_frameQueueIndex];

    foreach(QnAbstractRenderer* render, m_renderList) {
        if (render->isDisplaying(outFrame))
            render->waitForFrameDisplayed(data->channelNumber);
    }

    outFrame->channel = data->channelNumber;
    outFrame->flags = 0;

    outFrame->setUseExternalData(!enableFrameQueue && !useTmpFrame);

    m_mtx.lock();

    if (data->flags & QnAbstractMediaData::MediaFlags_AfterEOF)
    {
        foreach(QnAbstractRenderer* render, m_renderList)
            render->waitForFrameDisplayed(0);
        dec->resetDecoder(data);
    }

    if ((data->flags & AV_REVERSE_BLOCK_START) && m_decodeMode != QnAbstractVideoDecoder::DecodeMode_Fastest)
    {
        QnWritableCompressedVideoDataPtr emptyData(new QnWritableCompressedVideoData(1,0));
        while (dec->decode(emptyData, &m_tmpFrame))
        {
            if (scaleFactor == QnFrameScaler::factor_unknown && dec->getWidth() > 0)
            {
                scaleFactor = determineScaleFactor(
                    m_renderList,
                    data->channelNumber,
                    dec->getWidth(),
                    dec->getHeight(),
                    force_factor);
            }

            QSharedPointer<CLVideoDecoderOutput> tmpOutFrame( new CLVideoDecoderOutput() );
            if (!downscaleFrame(m_tmpFrame, tmpOutFrame, scaleFactor, pixFmt))
                continue;

            tmpOutFrame->channel = data->channelNumber;
            tmpOutFrame->flags |= QnAbstractMediaData::MediaFlags_Reverse;
            if (data->flags & QnAbstractMediaData::MediaFlags_LowQuality)
                tmpOutFrame->flags |= QnAbstractMediaData::MediaFlags_LowQuality; // flag unknown. set same flags as input data
            //tmpOutFrame->pkt_dts = AV_NOPTS_VALUE;
            m_reverseQueue.enqueue(tmpOutFrame);
            m_reverseSizeInBytes += avpicture_get_size((AVPixelFormat)tmpOutFrame->format, tmpOutFrame->width, tmpOutFrame->height);
            checkQueueOverflow(dec);
        }
        m_flushedBeforeReverseStart = true;
        reorderPrevFrames();
        if (!m_queueUsed) {
            foreach(QnAbstractRenderer* render, m_renderList)
                render->waitForFrameDisplayed(0); // codec frame may be displayed now
        }
        dec->resetDecoder(data);
    }

    QSharedPointer<CLVideoDecoderOutput> decodeToFrame = useTmpFrame ? m_tmpFrame : outFrame;
    decodeToFrame->flags = 0;
    if (!dec || !dec->decode(data, &decodeToFrame))
    {
        m_mtx.unlock();
        if (m_decodeMode == QnAbstractVideoDecoder::DecodeMode_Fastest)
            return Status_Skipped;
        if (!m_reverseQueue.isEmpty() && (m_reverseQueue.front()->flags & AV_REVERSE_REORDERED)) {
            outFrame = m_reverseQueue.dequeue();
            if (outFrame->data[0])
                m_reverseSizeInBytes -= avpicture_get_size((AVPixelFormat)outFrame->format, outFrame->width, outFrame->height);

            calcSampleAR(outFrame, dec);

            if (processDecodedFrame(dec, outFrame, enableFrameQueue, reverseMode))
                return Status_Displayed;
            else
                return Status_Buffered;
        }
        if (m_bufferedFrameDisplayer)
        {
            dec->setLightCpuMode(QnAbstractVideoDecoder::DecodeMode_Full); // do not skip more 1 frame in a row
            return Status_Buffered;
        }
        else
            return Status_Skipped;
    }
    m_mtx.unlock();
    m_rawDataSize = QSize(decodeToFrame->width,decodeToFrame->height);
    if (decodeToFrame->width) {
        if (qFuzzyIsNull(m_overridenAspectRatio)) {
            //qreal sampleAr = decodeToFrame->height > 0 ? (qreal)decodeToFrame->width / (qreal)decodeToFrame->height : 1.0;
            QSize imageSize(decodeToFrame->width * dec->getSampleAspectRatio(), decodeToFrame->height);
            QnMutexLocker lock( &m_imageSizeMtx );
            m_imageSize = imageSize;
        }
        else {
            QSize imageSize(decodeToFrame->height*m_overridenAspectRatio, decodeToFrame->height);
            QnMutexLocker lock( &m_imageSizeMtx );
            m_imageSize = imageSize;
        }
    }

    /*
    if (qAbs(decodeToFrame->pkt_dts-data->timestamp) > 500*1000) {
        // prevent large difference after seek or EOF
        outFrame->pkt_dts = data->timestamp;
    }
    */


    if (m_flushedBeforeReverseStart) {
        data->flags |= AV_REVERSE_BLOCK_START;
        m_flushedBeforeReverseStart = false;
    }

    pixFmt = dec->GetPixelFormat();
    if (scaleFactor == QnFrameScaler::factor_unknown) {
        // for stiil images decoder parameters are not know before decoding, so recalculate parameters
        // It is got one more data copy from tmpFrame, but for movies we have got valid dst pointer (tmp or not) immediate before decoding
        // It is necessary for new logic with display queue
        scaleFactor = determineScaleFactor(m_renderList, data->channelNumber, dec->getWidth(), dec->getHeight(), force_factor);
    }

    if (!draw || m_renderList.isEmpty())
        return Status_Skipped;
    else if (m_lastIgnoreTime != (qint64)AV_NOPTS_VALUE && decodeToFrame->pkt_dts <= m_lastIgnoreTime)
        return Status_Skipped;

    if (useTmpFrame)
    {
        //checkig once again for need to scale, since previous check could be incorrect due to unknown pixel format (this actual for some images, e.g., jpeg)
        const bool scalingStillNeeded =
            (dec->targetMemoryType() == QnAbstractPictureDataRef::pstSysMemPic) &&
            (!QnGLRenderer::isPixelFormatSupported(pixFmt) ||
             !CLVideoDecoderOutput::isPixelFormatSupported(pixFmt) ||
             scaleFactor != QnFrameScaler::factor_1);

        if( !scalingStillNeeded )
        {
            if( outFrame->isExternalData() )
                outFrame->reallocate( m_tmpFrame->width, m_tmpFrame->height, m_tmpFrame->format );
            CLVideoDecoderOutput::copy( m_tmpFrame.data(), outFrame.data() );
        }
        else if( !(dec->getDecoderCaps() & QnAbstractVideoDecoder::decodedPictureScaling) )
        {
            if (!downscaleFrame(m_tmpFrame, outFrame, scaleFactor, pixFmt))
                return Status_Displayed;
        }
        else
        {
            outFrame = m_tmpFrame;
        }
        outFrame->pkt_dts = m_tmpFrame->pkt_dts;
        outFrame->metadata = m_tmpFrame->metadata;
        outFrame->flags = m_tmpFrame->flags;
        outFrame->channel = data->channelNumber;
    }
    outFrame->flags |= data->flags;
    //outFrame->pts = data->timestamp;
    if (reverseMode)
    {
        if (outFrame->flags & AV_REVERSE_BLOCK_START)
            reorderPrevFrames();
        m_reverseQueue.enqueue(outFrame);
        m_reverseSizeInBytes += avpicture_get_size((AVPixelFormat)outFrame->format, outFrame->width, outFrame->height);
        checkQueueOverflow(dec);
        m_frameQueue[m_frameQueueIndex] = QSharedPointer<CLVideoDecoderOutput>( new CLVideoDecoderOutput() );
        if (!(m_reverseQueue.front()->flags & AV_REVERSE_REORDERED))
            return Status_Buffered; // frame does not ready. need more frames. does not perform wait
        outFrame = m_reverseQueue.dequeue();
        if (outFrame->data[0])
            m_reverseSizeInBytes -= avpicture_get_size((AVPixelFormat)outFrame->format, outFrame->width, outFrame->height);
    }

    calcSampleAR(outFrame, dec);

    if (processDecodedFrame(dec, outFrame, enableFrameQueue, reverseMode))
        return Status_Displayed;
    else
        return Status_Buffered;
}

bool QnVideoStreamDisplay::downscaleFrame(const CLVideoDecoderOutputPtr& src, const CLVideoDecoderOutputPtr& dst, QnFrameScaler::DownscaleFactor scaleFactor, AVPixelFormat pixFmt)
{
    if (QnGLRenderer::isPixelFormatSupported(pixFmt) && CLVideoDecoderOutput::isPixelFormatSupported(pixFmt) && scaleFactor <= QnFrameScaler::factor_8)
        QnFrameScaler::downscale(src.data(), dst.data(), scaleFactor); // fast scaler
    else {
        if (!rescaleFrame(*(src.data()), *dst, src->width / scaleFactor, src->height / scaleFactor)) // universal scaler
            return false;
    }
    dst->pkt_dts = src->pkt_dts;
    dst->pkt_pts = src->pkt_pts;
    dst->metadata = src->metadata;
    dst->flags = src->flags;
    dst->channel = src->channel;

    return true;
}

QnVideoStreamDisplay::FrameDisplayStatus QnVideoStreamDisplay::flushFrame(int channel, QnFrameScaler::DownscaleFactor force_factor)
{
    // use only 1 frame for non selected video
    if (m_reverseMode || m_decoder.isEmpty() || m_needResetDecoder)
        return Status_Skipped;

    QSharedPointer<CLVideoDecoderOutput> m_tmpFrame(new CLVideoDecoderOutput());
    m_tmpFrame->setUseExternalData(true);

    QnAbstractVideoDecoder* dec = m_decoder.begin().value();


    QnFrameScaler::DownscaleFactor scaleFactor = QnFrameScaler::factor_unknown;
    if (dec->getWidth() > 0) {
        scaleFactor = determineScaleFactor(m_renderList, channel, dec->getWidth(), dec->getHeight(), force_factor);
    }

    QSharedPointer<CLVideoDecoderOutput> outFrame = m_frameQueue[m_frameQueueIndex];

    foreach(QnAbstractRenderer* render, m_renderList)
        render->finishPostedFramesRender(channel);
    outFrame->channel = channel;

    m_mtx.lock();

    if (!dec->decode(QnCompressedVideoDataPtr(), &m_tmpFrame))
    {
        m_mtx.unlock();
        return Status_Skipped;
    }
    m_mtx.unlock();

    if (scaleFactor == QnFrameScaler::factor_unknown) {
        scaleFactor = determineScaleFactor(m_renderList, channel, dec->getWidth(), dec->getHeight(), force_factor);
    }

    AVPixelFormat pixFmt = dec->GetPixelFormat();
    if (!downscaleFrame(m_tmpFrame, outFrame, scaleFactor, pixFmt))
        return Status_Displayed;

    calcSampleAR(outFrame, dec);

    if (processDecodedFrame(dec, outFrame, false, false))
        return Status_Displayed;
    else
        return Status_Buffered;
}

bool QnVideoStreamDisplay::processDecodedFrame(
    QnAbstractVideoDecoder* decoder,
    const QSharedPointer<CLVideoDecoderOutput>& outFrame,
    bool enableFrameQueue,
    bool /*reverseMode*/)
{
    if (!outFrame->data[0] && !outFrame->picData)
        return false;

    if (enableFrameQueue)
    {
        NX_ASSERT(!outFrame->isExternalData());

        if (m_bufferedFrameDisplayer)
        {
            const bool wasWaiting = m_bufferedFrameDisplayer->addFrame(outFrame);
            const qint64 bufferedDuration = m_bufferedFrameDisplayer->bufferedDuration();
            if (wasWaiting)
            {
                decoder->setLightCpuMode(QnAbstractVideoDecoder::DecodeMode_Full);
                m_queueWasFilled = true;
            }
            else
            {
                if (m_queueWasFilled && bufferedDuration <= MAX_QUEUE_TIME / 4)
                    decoder->setLightCpuMode(QnAbstractVideoDecoder::DecodeMode_Fast);
            }
        }
        else
        {
            if (std::abs(static_cast<double>(m_speed)) < 1.0 + FPS_EPS)
            {
                for (const auto render: m_renderList)
                {
                    // Wait for old frame.
                    render->waitForQueueLessThan(outFrame->channel, kMaxFrameQueueSize);
                }
            }
            for (const auto render: m_renderList)
                render->draw(outFrame); //< Send the new one.

            processMetadata(outFrame->metadata, outFrame->channel);
        }
        // Allow frame queue for selected video.
        m_frameQueueIndex = (m_frameQueueIndex + 1) % kMaxFrameQueueSize;
        m_queueUsed = true;
    }
    else
    {
        for (const auto render: m_renderList)
            render->draw(outFrame);

        processMetadata(outFrame->metadata, outFrame->channel);

        for (const auto render: m_renderList)
            render->waitForFrameDisplayed(outFrame->channel);
    }
    m_lastDisplayedFrame = outFrame;

    return true;
}

void QnVideoStreamDisplay::processMetadata(const FrameMetadata& metadataList, int channel)
{
    QnMutexLocker lock(&m_metadataConsumersHashMutex);
    const auto consumers = m_metadataConsumerByType;
    lock.unlock();

    for (const auto& metadata: metadataList)
    {
        if (const auto consumer = m_metadataConsumerByType.value(metadata->metadataType).lock())
            consumer->processMetadata(metadata);
    }
}

bool QnVideoStreamDisplay::selfSyncUsed() const
{
    return m_bufferedFrameDisplayer;
}

qreal QnVideoStreamDisplay::overridenAspectRatio() const {
    return m_overridenAspectRatio;
}

void QnVideoStreamDisplay::setOverridenAspectRatio(qreal aspectRatio) {
    m_overridenAspectRatio = aspectRatio;
}

void QnVideoStreamDisplay::flushFramesToRenderer()
{
    foreach(QnAbstractRenderer* render, m_renderList)
        render->finishPostedFramesRender( m_channelNumber );
}

bool QnVideoStreamDisplay::rescaleFrame(const CLVideoDecoderOutput& srcFrame, CLVideoDecoderOutput& outFrame, int newWidth, int newHeight)
{
    static const int ROUND_FACTOR = 16;
    // due to openGL requirements chroma MUST be devided by 4, luma MUST be devided by 8
    // due to MMX scaled functions requirements chroma MUST be devided by 8, so luma MUST be devided by 16
    newWidth = qPower2Ceil((unsigned)newWidth, ROUND_FACTOR);

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

    if (outFrame.isExternalData() || outFrame.width != newWidth || outFrame.height != newHeight || outFrame.format != AV_PIX_FMT_RGBA)
        outFrame.reallocate(newWidth, newHeight, AV_PIX_FMT_RGBA);

    sws_scale(m_scaleContext,srcFrame.data, srcFrame.linesize, 0,
        srcFrame.height, outFrame.data, outFrame.linesize);
    return true;
}

void QnVideoStreamDisplay::setLightCPUMode(QnAbstractVideoDecoder::DecodeMode val)
{
    m_decodeMode = val;
    QnMutexLocker mutex( &m_mtx );

    foreach(QnAbstractVideoDecoder* decoder, m_decoder)
    {
        decoder->setLightCpuMode(val);
    }
}

QnFrameScaler::DownscaleFactor QnVideoStreamDisplay::findScaleFactor(int width, int height, int fitWidth, int fitHeight)
{
    if (fitWidth * 8 <= width  && fitHeight * 8 <= height)
        return QnFrameScaler::factor_8;
    if (fitWidth * 4 <= width  && fitHeight * 4 <= height)
        return QnFrameScaler::factor_4;
    else if (fitWidth * 2 <= width  && fitHeight * 2 <= height)
        return QnFrameScaler::factor_2;
    else
        return QnFrameScaler::factor_1;
}

void QnVideoStreamDisplay::setMTDecoding(bool value)
{
    QnMutexLocker lock(&m_mtx);
    m_enableFrameQueue = value;
    m_needReinitDecoders = true;
}

void QnVideoStreamDisplay::setSpeed(float value)
{
    m_speed = value;
    m_reverseMode = value < 0;
    if (m_reverseMode)
    {
        QnMutexLocker lock(&m_mtx);
        m_enableFrameQueue = true;
    }

    QnMutexLocker lock( &m_mtx );
    for( QMap<AVCodecID, QnAbstractVideoDecoder*>::const_iterator
        it = m_decoder.begin();
        it != m_decoder.end();
        ++it )
    {
        it.value()->setSpeed( value );
    }

    //if (qAbs(m_speed) > 1.0+FPS_EPS)
    //    m_enableFrameQueue = true;
}

/*
qint64 QnVideoStreamDisplay::getTimestampOfNextFrameToRender() const
{
    QnMutexLocker lock( &m_timeMutex );
    if (m_drawer && m_timeChangeEnabled)
        return m_drawer->lastDisplayedTime(m_channelNumber);
    else
        return m_lastDisplayedTime;
}
*/

void QnVideoStreamDisplay::overrideTimestampOfNextFrameToRender(qint64 value)
{
    foreach(QnAbstractRenderer* render, m_renderList)
    {
        render->blockTimeValue(m_channelNumber, value);
        if (m_bufferedFrameDisplayer)
            m_bufferedFrameDisplayer->clear();
        render->finishPostedFramesRender(m_channelNumber);
        render->unblockTimeValue(m_channelNumber);
    }
}

qint64 QnVideoStreamDisplay::getTimestampOfNextFrameToRender() const
{
    if (m_renderList.isEmpty())
        return AV_NOPTS_VALUE;
    foreach(QnAbstractRenderer* renderer, m_renderList)
    {
        QnResourceWidgetRenderer* r = dynamic_cast<QnResourceWidgetRenderer*>(renderer);
        if (r && r->isEnabled(m_channelNumber))
            return r->getTimestampOfNextFrameToRender(m_channelNumber);
    }

    QnAbstractRenderer* renderer = *m_renderList.begin();
    return renderer->getTimestampOfNextFrameToRender(m_channelNumber);
}

void QnVideoStreamDisplay::blockTimeValue(qint64 time)
{
    foreach(QnAbstractRenderer* render, m_renderList)
        render->blockTimeValue(m_channelNumber, time);
}

void QnVideoStreamDisplay::setPausedSafe(bool value)
{
    QnMutexLocker lock( &m_renderListMtx );
    foreach(QnAbstractRenderer* render, m_renderList)
        render->setPaused(value);
    foreach(QnAbstractRenderer* render, m_newList)
        render->setPaused(value);
    m_isPaused = value;
}

void QnVideoStreamDisplay::blockTimeValueSafe(qint64 time)
{
    QnMutexLocker lock( &m_renderListMtx );
    foreach(QnAbstractRenderer* render, m_renderList)
        render->blockTimeValue(m_channelNumber, time);
}

bool QnVideoStreamDisplay::isTimeBlocked() const
{
    QnMutexLocker lock(&m_renderListMtx);
    if (m_renderList.isEmpty())
        return false;
    QnAbstractRenderer* renderer = *m_renderList.begin();
    return renderer->isTimeBlocked(m_channelNumber);
}

void QnVideoStreamDisplay::unblockTimeValue()
{
    /*
    QnMutexLocker lock( &m_timeMutex );
    if (m_bufferedFrameDisplayer)
        m_bufferedFrameDisplayer->overrideTimestampOfNextFrameToRender(m_lastDisplayedTime);
    m_timeChangeEnabled = true;
    */
    foreach(QnAbstractRenderer* render, m_renderList)
        render->unblockTimeValue(m_channelNumber);
}

void QnVideoStreamDisplay::afterJump()
{
    clearReverseQueue();
    if (m_bufferedFrameDisplayer)
        m_bufferedFrameDisplayer->clear();
    foreach(QnAbstractRenderer* render, m_renderList)
        render->finishPostedFramesRender(m_channelNumber);
    m_needResetDecoder = true;
    //qDebug() << "after jump, clear all frames";

    //for (QMap<AVCodecID, CLAbstractVideoDecoder*>::iterator itr = m_decoder.begin(); itr != m_decoder.end(); ++itr)
    //    (*itr)->resetDecoder();
    m_queueWasFilled = false;
    m_lastIgnoreTime = AV_NOPTS_VALUE;
}

void QnVideoStreamDisplay::onNoVideo()
{
    foreach(QnAbstractRenderer* render, m_renderList)
        render->onNoVideo();
}

void QnVideoStreamDisplay::clearReverseQueue()
{
    foreach(QnAbstractRenderer* render, m_renderList)
        render->finishPostedFramesRender(0);

    QnMutexLocker lock( &m_mtx );
    m_reverseQueue.clear();
    m_reverseSizeInBytes = 0;
}

QImage QnVideoStreamDisplay::getGrayscaleScreenshot()
{
    if (m_decoder.isEmpty())
        return QImage();
    QnAbstractVideoDecoder* dec = m_decoder.begin().value();
    QnMutexLocker mutex( &m_mtx );
    const AVFrame* lastFrame = dec->lastFrame();
    if (m_reverseMode && m_lastDisplayedFrame && m_lastDisplayedFrame->data[0])
        lastFrame = m_lastDisplayedFrame.data();

    if (!lastFrame || !lastFrame->width || !lastFrame->data[0])
        return QImage();

    QImage tmp(lastFrame->data[0], lastFrame->width, lastFrame->height, lastFrame->linesize[0], QImage::Format_Indexed8);
    QImage rez( lastFrame->width, lastFrame->height, QImage::Format_Indexed8);
    rez = tmp.copy(0,0, lastFrame->width, lastFrame->height);
    return rez;
}


CLVideoDecoderOutputPtr QnVideoStreamDisplay::getScreenshot(bool anyQuality)
{
    QnMutexLocker mutex( &m_mtx );

    if (!m_lastDisplayedFrame || !m_lastDisplayedFrame->data[0] || !m_lastDisplayedFrame->width)
        return CLVideoDecoderOutputPtr();

    // feature #2563
    if (!anyQuality && (m_lastDisplayedFrame->flags & QnAbstractMediaData::MediaFlags_LowQuality))
        return CLVideoDecoderOutputPtr();    //screenshot will be received from the server

#if 0
    /* Do not take local screenshot if displayed frame has different size.
       Checking only height because width can be modified by forced AR. */
    if (!anyQuality && m_lastDisplayedFrame->height != m_imageSize.height())
        return CLVideoDecoderOutputPtr();    //screenshot will be received from the server
#endif

    CLVideoDecoderOutputPtr outFrame(new CLVideoDecoderOutput());
    if (!m_decoder.isEmpty())
        getLastDecodedFrame(m_decoder.begin().value(), &outFrame);
    else
        CLVideoDecoderOutput::copy(m_lastDisplayedFrame.data(), outFrame.data());
    outFrame->channel = m_lastDisplayedFrame->channel;
    return outFrame;
}

void QnVideoStreamDisplay::setCurrentTime(qint64 time)
{
    if (m_bufferedFrameDisplayer)
        m_bufferedFrameDisplayer->setCurrentTime(time);
}

void QnVideoStreamDisplay::canUseBufferedFrameDisplayer(bool value)
{
    m_canUseBufferedFrameDisplayer = value;
}

QSize QnVideoStreamDisplay::getImageSize() const
{
    QnMutexLocker lock( &m_imageSizeMtx );
    return m_imageSize;
}

bool QnVideoStreamDisplay::getLastDecodedFrame( QnAbstractVideoDecoder* dec, QSharedPointer<CLVideoDecoderOutput>* const outFrame )
{
    const AVFrame* lastFrame = dec->lastFrame();
    if (!lastFrame || !lastFrame->data[0] || dec->GetPixelFormat() == -1 || dec->getWidth() == 0)
        return false;

    (*outFrame)->setUseExternalData( false );
    (*outFrame)->reallocate( dec->getWidth(), dec->getHeight(), dec->GetPixelFormat(), lastFrame->linesize[0] );

    //TODO/IMPL it is possible to avoid copying in this method and simply return shared pointer to lastFrame, but this will require
        //QnAbstractVideoDecoder::lastFrame() to return QSharedPointer<CLVideoDecoderOutput> and
        //tracking of frame usage in decoder

#if 0
    // todo: ffmpeg-test. deinterlace
    if( lastFrame->interlaced_frame && dec->isMultiThreadedDecoding() )
    {
        avpicture_deinterlace( (AVPicture*) outFrame->data(), (AVPicture*) lastFrame, dec->GetPixelFormat(), dec->getWidth(), dec->getHeight() );
        (*outFrame)->pkt_dts = lastFrame->pkt_dts;
    }
    else
#endif
    {
        if( (*outFrame)->format == AV_PIX_FMT_YUV420P )
        {
            // optimization
            for (int i = 0; i < 3 && lastFrame->data[i]; ++i)
            {
                int h = lastFrame->height >> (i > 0 ? 1 : 0);
                memcpy( (*outFrame)->data[i], lastFrame->data[i], lastFrame->linesize[i]* h );
            }
        }
        else
        {
            av_picture_copy( (AVPicture*)outFrame->data(), (AVPicture*)lastFrame, dec->GetPixelFormat(), dec->getWidth(), dec->getHeight() );
        }
        (*outFrame)->pkt_dts = lastFrame->pkt_dts;
    }

    (*outFrame)->format = dec->GetPixelFormat();
    return true;
}

QSize QnVideoStreamDisplay::getMaxScreenSize() const
{
    QnMutexLocker lock(&m_renderListMtx);
    int maxW = 0, maxH = 0;
    foreach(QnAbstractRenderer* render, m_renderList)
    {
        QSize sz = render->sizeOnScreen(0);
        maxW = qMax(sz.width(), maxW);
        maxH = qMax(sz.height(), maxH);
    }
    return QSize(maxW, maxH);
}
