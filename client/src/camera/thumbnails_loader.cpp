#include "thumbnails_loader.h"
#include "decoders/video/ffmpeg.h"
#include "utils/common/synctime.h"

// ------------------------ QnThumbnailsLoader -------------------------

QnThumbnailsLoader::QnThumbnailsLoader(QnResourcePtr resource):
    m_mutex(QMutex::Recursive),
    m_rtspClient(new QnRtspClientArchiveDelegate())
{
    m_resource = resource;
    m_step = 0;
    m_startTime = 0;
    m_endTime = 0;
    m_outWidth = 128;
    m_outHeight = 128*3/4;
    m_scaleContext = 0;
    m_rgbaBuffer = 0;
    m_prevOutWidth = 0;
    m_prevOutHeight = 0;

    m_srcLineSize = 0;
    m_srcWidth = 0;
    m_srcHeight = 0;
    m_srcFormat = 0;
    m_lastLoadedTime = 0;
    m_breakCurrentJob = false;

    start();
}

QnThumbnailsLoader::~QnThumbnailsLoader()
{
    pleaseStop();
    stop();
    if (m_scaleContext)
        sws_freeContext(m_scaleContext);
    qFreeAligned(m_rgbaBuffer);
}

void QnThumbnailsLoader::setThumbnailsSize(int width, int height)
{
    m_outWidth = width;
    m_outHeight = height;
}

QnTimePeriod QnThumbnailsLoader::loadedRange() const
{
    QMutexLocker locker(&m_mutex);
    return QnTimePeriod(m_startTime, m_endTime - m_startTime);
}

void QnThumbnailsLoader::loadRange(qint64 startTimeMs, qint64 endTimeMs, qint64 stepMs)
{
    QMutexLocker locker(&m_mutex);
    QnTimePeriod oldPeriod(m_startTime, m_endTime - m_startTime);
    QnTimePeriod newPeriod(startTimeMs, endTimeMs - startTimeMs);
    if (stepMs != m_step || oldPeriod.intersect(newPeriod).isEmpty())
    {
        m_step = stepMs;
        m_breakCurrentJob = true;
        m_rtspClient->setAdditionalAttribute("x-media-step", QByteArray::number(m_step*1000));
        m_newImages.clear();
        m_rangeToLoad.clear();
        m_startTime = m_endTime = 0;
    }
    else {
        m_startTime = qMax(m_startTime, startTimeMs);
        m_endTime = qMin(m_endTime, endTimeMs);
    }

    addRange(startTimeMs, endTimeMs, stepMs);
}

void QnThumbnailsLoader::addRange(qint64 startTimeMs, qint64 endTimeMs, qint64 stepMs)
{
    QMutexLocker locker(&m_mutex);
    m_step = stepMs;

    if (m_startTime == 0) {
        m_rangeToLoad << QnTimePeriod(startTimeMs, endTimeMs - startTimeMs);
        m_startTime = startTimeMs;
        m_endTime = endTimeMs;
    }
    else {
        if (startTimeMs < m_startTime)
        {
            m_rangeToLoad << QnTimePeriod(startTimeMs, m_startTime - startTimeMs);
            m_startTime = startTimeMs;
        }
        if (endTimeMs > m_endTime)
        {
            m_rangeToLoad << QnTimePeriod(m_endTime, endTimeMs - m_endTime);
            m_endTime = endTimeMs;
        }
    }
    m_lastLoadedTime = qnSyncTime->currentMSecsSinceEpoch();
}

qint64 QnThumbnailsLoader::lastLoadingTime() const
{
    return m_lastLoadedTime;
}

void QnThumbnailsLoader::removeRange(qint64 startTimeMs, qint64 endTimeMs)
{
    QMutexLocker locker(&m_mutex);
    QMap<qint64, QPixmap>::iterator itr = m_newImages.lowerBound(startTimeMs);
    QMap<qint64, QPixmap>::iterator endPos = m_newImages.lowerBound(endTimeMs);
    while (itr != endPos) {
        itr = m_newImages.erase(itr);
    }
}

const QPixmap *QnThumbnailsLoader::getPixmapByTime(qint64 timeMs, quint64* realPixmapTimeMs)
{
    QMutexLocker locker(&m_mutex);
    QMap<qint64, QPixmap>::iterator itrUp = m_images.lowerBound(timeMs);
    QMap<qint64, QPixmap>::iterator itrDown = m_images.lowerBound(timeMs - m_step);
    QMap<qint64, QPixmap>::iterator bestItr;
    if (itrUp != m_images.end() && qAbs(itrUp.key() - timeMs) <= qAbs(itrDown.key() - timeMs))
        bestItr = itrUp;
    else
        bestItr = itrDown;
    
    if (bestItr == m_images.end() || qAbs(bestItr.key() - timeMs) >= m_step) {
        if (realPixmapTimeMs)
            *realPixmapTimeMs = -1;

        if (bestItr != m_images.end())
        {
            qint64 eps = qAbs(bestItr.key() - timeMs);
            eps = eps;
        }

        return 0;
    }
    else {
        if (realPixmapTimeMs)
            *realPixmapTimeMs = bestItr.key();
        return &(bestItr.value());
    }
}

void QnThumbnailsLoader::allocateScaleContext(int lineSize, int width, int height, PixelFormat format)
{
    if (m_scaleContext) 
    {
        if (m_srcLineSize == lineSize && m_srcWidth == width && m_srcHeight == height && m_srcFormat == format && m_prevOutWidth == m_outWidth && m_prevOutHeight == m_outHeight)
            return;

        sws_freeContext(m_scaleContext);
        m_scaleContext = 0;
        m_srcLineSize = lineSize;
        m_srcWidth = width;
        m_srcHeight = height;
        m_srcFormat = format;
        m_prevOutWidth = m_outWidth;
        m_prevOutHeight = m_outHeight;
    }
    qFreeAligned(m_rgbaBuffer);

    int numBytes = avpicture_get_size(PIX_FMT_RGBA, qPower2Ceil((quint32) m_outWidth,8), m_outHeight);
    m_rgbaBuffer = (quint8*) qMallocAligned(numBytes, 32);
    m_scaleContext = sws_getContext(width, height, format, 
        m_outWidth, m_outHeight, PIX_FMT_BGRA, SWS_POINT, NULL, NULL, NULL);
}

bool QnThumbnailsLoader::processFrame(const CLVideoDecoderOutput& outFrame)
{
    int dstLineSize[4];
    quint8* dstBuffer[4];

    allocateScaleContext(outFrame.linesize[0], outFrame.width, outFrame.height, (PixelFormat) outFrame.format);
    dstLineSize[0] = qPower2Ceil((quint32) m_outWidth*4, 32);
    dstLineSize[1] = dstLineSize[2] = dstLineSize[3] = 0;
    QImage image(m_rgbaBuffer, m_outWidth, m_outHeight, dstLineSize[0], QImage::Format_ARGB32_Premultiplied);
    dstBuffer[0] = dstBuffer[1] = dstBuffer[2] = dstBuffer[3] = m_rgbaBuffer;

    sws_scale(m_scaleContext,
        outFrame.data, outFrame.linesize, 
        0, outFrame.height, 
        dstBuffer, dstLineSize);

    QMutexLocker lock(&m_mutex);
    if (m_breakCurrentJob)
        return false;

    QPixmap pixmap = QPixmap::fromImage(image);
    m_newImages.insert(outFrame.pkt_dts/1000, pixmap);
    emit gotNewPixmap(outFrame.pkt_dts/1000, pixmap);

    return true;
}

void QnThumbnailsLoader::run()
{
    while (!m_needStop)
    {
        if (m_rangeToLoad.isEmpty() || m_outWidth == 0 || m_outHeight == 0)
        {
            msleep(5);
            continue;
        }
        if (!m_rtspClient->open(m_resource))
        {
            msleep(500);
            continue;
        }

        
        m_mutex.lock();
        QnTimePeriod period = m_rangeToLoad.dequeue();
        m_breakCurrentJob = false;
        m_mutex.unlock();

        m_rtspClient->seek(period.startTimeMs*1000, period.endTimeMs()*1000);

        QnCompressedVideoDataPtr frame = qSharedPointerDynamicCast<QnCompressedVideoData> (m_rtspClient->getNextData());
        if (!frame) 
            continue;

        CLFFmpegVideoDecoder decoder(frame->compressionType, frame, false);
        CLVideoDecoderOutput outFrame;
        outFrame.setUseExternalData(false);

        while (!m_needStop && frame)
        {
            if (decoder.decode(frame, &outFrame))
            {
                if (!processFrame(outFrame))
                    break;
            }

            frame = qSharedPointerDynamicCast<QnCompressedVideoData> (m_rtspClient->getNextData());
        }

        QnCompressedVideoDataPtr emptyData(new QnCompressedVideoData(1,0));
        while (!m_needStop && decoder.decode(emptyData, &outFrame))
            processFrame(outFrame);

        m_rtspClient->close();
        if (m_rangeToLoad.isEmpty())
        {
            QMutexLocker lock(&m_mutex);
            removeRange(0, m_startTime);
            removeRange(m_endTime, INT64_MAX);
            m_images = m_newImages;
        }
    }
}

void QnThumbnailsLoader::pleaseStop()
{
    m_rtspClient->beforeClose();
    m_rtspClient->close();
}

qint64 QnThumbnailsLoader::step() const
{
    return m_step;
}

void QnThumbnailsLoader::lockPixmaps()
{
    m_mutex.lock();
}

void QnThumbnailsLoader::unlockPixmaps()
{
    m_mutex.unlock();
}
