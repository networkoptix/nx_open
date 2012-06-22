#include "thumbnails_loader.h"

#include <cassert>

#include <QtCore/QTimer>
#include <QtGui/QPixmap>
#include <QtGui/QImage>

#include <utils/common/math.h>
#include <utils/common/synctime.h>

#include "decoders/video/ffmpeg.h"

#include "ui/common/geometry.h"

#include "plugins/resources/archive/archive_stream_reader.h"
#include "device_plugins/archive/rtsp/rtsp_client_archive_delegate.h"
#include "utils/media/frame_info.h"

#include "thumbnails_loader_helper.h"

namespace {
    const qint64 defaultUpdateInterval = 60 * 1000; /* One minute. */

}


QnThumbnailsLoader::QnThumbnailsLoader(QnResourcePtr resource):
    m_mutex(QMutex::NonRecursive),
    m_rtspClient(new QnRtspClientArchiveDelegate()),
    m_resource(resource),
    m_timeStep(0),
    m_requestStart(0),
    m_requestEnd(0),
    m_processingStart(0),
    m_processingEnd(0),
    m_scaleContext(NULL),
    m_scaleBuffer(NULL),
    m_boundingSize(128, 96), /* That's 4:3 aspect ratio. */
    m_scaleSourceSize(0, 0),
    m_scaleTargetSize(0, 0),
    m_scaleSourceLine(0),
    m_scaleSourceFormat(0),
    m_helper(NULL),
    m_generation(0)
{
    static volatile bool metaTypesInitialized = false;
    if (!metaTypesInitialized) {
        qRegisterMetaType<QnThumbnail>();
        metaTypesInitialized = true;
    }

    start();
}

QnThumbnailsLoader::~QnThumbnailsLoader() {
    pleaseStop();
    stop();
    if (m_scaleContext)
        sws_freeContext(m_scaleContext);
    
    qFreeAligned(m_scaleBuffer);
}

QnResourcePtr QnThumbnailsLoader::resource() const {
    /* Resource is immutable, so we don't need a lock here. */

    return m_resource;
}

void QnThumbnailsLoader::setBoundingSize(const QSize &size) {
    QMutexLocker locker(&m_mutex);

    if(m_boundingSize == size)
        return;

    m_boundingSize = size;

    updateTargetSizeLocked(true);
}

QSize QnThumbnailsLoader::boundingSize() const {
    QMutexLocker locker(&m_mutex);

    return m_boundingSize;
}

QSize QnThumbnailsLoader::thumbnailSize() const {
    QMutexLocker locker(&m_mutex);

    return m_scaleTargetSize;
}

qint64 QnThumbnailsLoader::timeStep() const {
    QMutexLocker locker(&m_mutex);

    return m_timeStep;
}

void QnThumbnailsLoader::setTimeStep(qint64 timeStep) {
    QMutexLocker locker(&m_mutex);

    if(m_timeStep == timeStep)
        return;

    m_timeStep = timeStep;

    invalidateThumbnailsLocked();
}

qint64 QnThumbnailsLoader::startTime() const {
    QMutexLocker locker(&m_mutex);

    return m_requestStart;
}

void QnThumbnailsLoader::setStartTime(qint64 startTime) {
    QMutexLocker locker(&m_mutex);

    setTimePeriodLocked(startTime, qMax(startTime, m_requestEnd));
}

qint64 QnThumbnailsLoader::endTime() const {
    QMutexLocker locker(&m_mutex);

    return m_requestStart;
}

void QnThumbnailsLoader::setEndTime(qint64 endTime) {
    QMutexLocker locker(&m_mutex);

    setTimePeriodLocked(qMin(m_requestStart, endTime), endTime);
}

void QnThumbnailsLoader::setTimePeriod(qint64 startTime, qint64 endTime) {
    QMutexLocker locker(&m_mutex);

    setTimePeriodLocked(startTime, endTime);
}

void QnThumbnailsLoader::setTimePeriod(const QnTimePeriod &timePeriod) {
    QMutexLocker locker(&m_mutex);

    setTimePeriodLocked(timePeriod.startTimeMs, timePeriod.endTimeMs());
}

void QnThumbnailsLoader::setTimePeriodLocked(qint64 startTime, qint64 endTime) {
    if(endTime < startTime)
        endTime = startTime;

    if(m_requestStart == startTime && m_requestEnd == endTime)
        return;

    m_requestStart = startTime;
    m_requestEnd = endTime;

    updateProcessingLocked();
}

QnTimePeriod QnThumbnailsLoader::timePeriod() const {
    QMutexLocker locker(&m_mutex);

    return QnTimePeriod(m_requestStart, m_requestEnd - m_requestStart);
}

void QnThumbnailsLoader::pleaseStop() {
    m_rtspClient->beforeClose();
    m_rtspClient->close();
    quit();

    base_type::pleaseStop();
}

void QnThumbnailsLoader::updateTargetSizeLocked(bool invalidate) {
    QSize targetSize;

    if(!m_scaleSourceSize.isEmpty() && !m_boundingSize.isEmpty())
        targetSize = QnGeometry::expanded(QnGeometry::aspectRatio(m_scaleSourceSize), m_boundingSize, Qt::KeepAspectRatio).toSize();

    if(m_targetSize == targetSize)
        return;

    m_targetSize = targetSize;

    if(invalidate)
        invalidateThumbnailsLocked();
}

void QnThumbnailsLoader::invalidateThumbnailsLocked() {
    m_thumbnailByTime.clear();
    m_processingQueue.clear();
    m_maxLoadedTime = 0;
    m_processingStart = m_processingEnd = 0;
    m_generation++;

    updateProcessingLocked();

    m_mutex.unlock();
    emit thumbnailsInvalidated();
    m_mutex.lock();
}

void QnThumbnailsLoader::updateProcessingLocked() {
    /* Don't load anything if request fits into processed period. */
    if(m_requestStart >= m_processingStart && m_requestEnd <= m_processingEnd)
        return;

    /* Also don't load anything if this loader is not yet initialized. */
    if(m_timeStep == 0 || m_boundingSize.isEmpty() || (m_requestStart == 0 && m_requestEnd == 0))
        return;

    /* Discard old loaded period if it cannot be used to extend then new one. */
    if((m_requestStart > m_processingEnd + m_timeStep || m_requestEnd < m_processingStart - m_timeStep) && m_processingStart != 0 && m_processingEnd != 0) {
        invalidateThumbnailsLocked();
        return;
    }

    /* Add margins. */
    qint64 processingStart = m_requestStart;
    qint64 processingEnd = m_requestEnd;
    qint64 processingSize = processingEnd - processingStart;
    processingStart = qFloor(qMax(0ll, processingStart - processingSize / 4), m_timeStep);
    processingEnd = qCeil(processingEnd + processingSize / 4, m_timeStep);

    /* Trim at live. */
    qint64 currentTime = qnSyncTime->currentMSecsSinceEpoch();
    if(processingEnd > currentTime)
        processingEnd = currentTime + defaultUpdateInterval;

    /* Adjust for the chunks near live that could not be loaded at the last request. */
    if(m_processingStart < m_maxLoadedTime && m_maxLoadedTime < m_processingEnd) {
        processingStart = qMin(processingStart, m_maxLoadedTime + m_timeStep);
        m_processingEnd = m_maxLoadedTime;
    }

    /* Enqueue ranges that are not loaded yet. */
    qint64 start, end;

    /* Try 1st option. */
    start = processingStart;
    end = qMin(m_processingStart - m_timeStep, processingEnd);
    if(start <= end)
        enqueueForProcessingLocked(start, end);

    /* Try 2nd option. */
    start = qMax(m_processingEnd + m_timeStep, processingStart);
    end = processingEnd;
    if(start <= end)
        enqueueForProcessingLocked(start, end);

    /* Update loaded period accordingly. */
    if(m_processingStart == 0 && m_processingEnd == 0) {
        m_processingStart = processingStart;
        m_processingEnd = processingEnd;
    } else {
        m_processingStart = qMin(m_processingStart, processingStart);
        m_processingEnd = qMax(m_processingEnd, processingEnd);
    }
}

void QnThumbnailsLoader::enqueueForProcessingLocked(qint64 startTime, qint64 endTime) {
    m_processingQueue.enqueue(QnTimePeriod(startTime, endTime - startTime));

    if(m_processingQueue.size() == 1)
        emit processingRequested();
}

void QnThumbnailsLoader::run() {
    assert(QThread::currentThread() == this); /* This function is supposed to be run from thumbnail rendering thread only. */

    m_helper = new QnThumbnailsLoaderHelper(this);
    connect(this, SIGNAL(processingRequested()), m_helper, SLOT(process()), Qt::QueuedConnection);
    connect(m_helper, SIGNAL(thumbnailLoaded(const QnThumbnail &)), this, SLOT(addThumbnail(const QnThumbnail &)));

    if(!m_processingQueue.empty())
        emit processingRequested();

    base_type::run();

    delete m_helper;
    m_helper = NULL;
}

void QnThumbnailsLoader::process() {
    QnTimePeriod period;
    QSize boundingSize;
    qint64 timeStep;
    int generation;

    {
        QMutexLocker locker(&m_mutex);

        if(m_processingQueue.isEmpty())
            return;

        period = m_processingQueue.dequeue();
        boundingSize = m_boundingSize;
        timeStep = m_timeStep;
        generation = m_generation;
    }

    m_rtspClient->setAdditionalAttribute("x-media-step", QByteArray::number(timeStep * 1000));

    if (!m_rtspClient->open(m_resource))
        QTimer::singleShot(500, m_helper, SLOT(process()));

    m_rtspClient->seek(period.startTimeMs * 1000, period.endTimeMs() * 1000);
    QnCompressedVideoDataPtr frame = m_rtspClient->getNextData().dynamicCast<QnCompressedVideoData>();
    if (frame) {
        CLFFmpegVideoDecoder decoder(frame->compressionType, frame, false);
        CLVideoDecoderOutput outFrame;
        outFrame.setUseExternalData(false);

        bool invalidated = false;
        while (frame) {
            if (decoder.decode(frame, &outFrame))
                generateThumbnail(outFrame, boundingSize, timeStep, generation);

            {
                QMutexLocker locker(&m_mutex);
                if(m_generation != generation) {
                    invalidated = true;
                    break;
                }
            }

            frame = qSharedPointerDynamicCast<QnCompressedVideoData> (m_rtspClient->getNextData());
        }

        if(!invalidated) {
            QnCompressedVideoDataPtr emptyData(new QnCompressedVideoData(1,0));
            while (decoder.decode(emptyData, &outFrame))
                generateThumbnail(outFrame, boundingSize, timeStep, generation);
        }
    }
    m_rtspClient->close();

    /*for(qint64 time = period.startTimeMs; time < period.endTimeMs(); time++)
        if(!m_thumbnailByTime)
            (QnThumbnail(QPixmap()));*/
}

void QnThumbnailsLoader::addThumbnail(const QnThumbnail &thumbnail) {
    {
        QMutexLocker locker(&m_mutex);

        if(thumbnail.generation() != m_generation)
            return; /* Already outdated. */

        m_thumbnailByTime.insert(thumbnail.time(), thumbnail);
        m_maxLoadedTime = qMax(thumbnail.time(), m_maxLoadedTime);
    }

    emit thumbnailLoaded(thumbnail);
}

void QnThumbnailsLoader::ensureScaleContextLocked(int lineSize, const QSize &sourceSize, const QSize &boundingSize, int format) {
    bool needsReallocation = true;
    
    if(m_scaleSourceSize != sourceSize) {
        m_scaleSourceSize = sourceSize;
        m_scaleSourceFormat = format;
        m_scaleSourceLine = lineSize;
        updateTargetSizeLocked(false);
    }

    if(m_scaleSourceLine == lineSize && m_scaleSourceSize == sourceSize && m_scaleSourceFormat == format && m_targetSize == m_scaleTargetSize)
        needsReallocation = false;

    if(!m_scaleContext)
        needsReallocation = true;

    if(needsReallocation) {
        if(m_scaleContext) {
            sws_freeContext(m_scaleContext);
            m_scaleContext = 0;

            qFreeAligned(m_scaleBuffer);
            m_scaleBuffer = NULL;
        }

        m_scaleSourceSize = sourceSize;
        m_scaleSourceFormat = format;
        m_scaleSourceLine = lineSize;
        m_scaleTargetSize = m_targetSize;

        int numBytes = avpicture_get_size(PIX_FMT_RGBA, qPower2Ceil(static_cast<quint32>(m_scaleTargetSize.width()), 8), m_scaleTargetSize.height());
        m_scaleBuffer = static_cast<quint8 *>(qMallocAligned(numBytes, 32));
        m_scaleContext = sws_getContext(m_scaleSourceSize.width(), m_scaleSourceSize.height(), static_cast<PixelFormat>(m_scaleSourceFormat), m_scaleTargetSize.width(), m_scaleTargetSize.height(), PIX_FMT_BGRA, SWS_POINT, NULL, NULL, NULL);
    }
}

void QnThumbnailsLoader::generateThumbnail(const CLVideoDecoderOutput &outFrame, const QSize &boundingSize, qint64 timeStep, int generation) {
    QSize scaleTargetSize;
    {
        QMutexLocker locker(&m_mutex);
        ensureScaleContextLocked(outFrame.linesize[0], QSize(outFrame.width, outFrame.height), boundingSize, (PixelFormat) outFrame.format);
        scaleTargetSize = m_scaleTargetSize;
    }

    int dstLineSize[4];
    quint8* dstBuffer[4];
    dstLineSize[0] = qPower2Ceil(static_cast<quint32>(scaleTargetSize.width() * 4), 32);
    dstLineSize[1] = dstLineSize[2] = dstLineSize[3] = 0;
    QImage image(m_scaleBuffer, scaleTargetSize.width(), scaleTargetSize.height(), dstLineSize[0], QImage::Format_ARGB32_Premultiplied);
    dstBuffer[0] = dstBuffer[1] = dstBuffer[2] = dstBuffer[3] = m_scaleBuffer;

    sws_scale(m_scaleContext, outFrame.data, outFrame.linesize, 0, outFrame.height, dstBuffer, dstLineSize);

    QPixmap pixmap(QPixmap::fromImage(image));

    qint64 actualTime = outFrame.pkt_dts / 1000;
    emit m_helper->thumbnailLoaded(QnThumbnail(pixmap, pixmap.size(), qRound(actualTime, timeStep), actualTime, timeStep, generation));
}

#if 0
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
    else 
    {
        m_startTime = qMax(m_startTime, startTimeMs);
        m_endTime = qMin(m_endTime, endTimeMs);
    }

    addRange(startTimeMs, endTimeMs, stepMs);
}

void QnThumbnailsLoader::addRange(qint64 startTimeMs, qint64 endTimeMs, qint64 stepMs)
{
    QMutexLocker locker(&m_mutex);
    m_step = stepMs;

    if (m_startTime == 0) 
    {
        m_rangeToLoad << QnTimePeriod(startTimeMs, endTimeMs - startTimeMs);
        m_startTime = startTimeMs;
        m_endTime = endTimeMs;
    }
    else 
    {
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

    m_lastLoadedTime = QDateTime::currentMSecsSinceEpoch();
}

qint64 QnThumbnailsLoader::currentMSecsSinceLastUpdate() const 
{
    return QDateTime::currentMSecsSinceEpoch() - m_lastLoadedTime;
}

void QnThumbnailsLoader::removeRange(qint64 startTimeMs, qint64 endTimeMs)
{
    QMutexLocker locker(&m_mutex);
    QMap<qint64, QPixmapPtr>::iterator itr = m_newImages.lowerBound(startTimeMs);
    QMap<qint64, QPixmapPtr>::iterator endPos = m_newImages.lowerBound(endTimeMs);
    while (itr != endPos)
        itr = m_newImages.erase(itr);
}

QPixmapPtr QnThumbnailsLoader::getPixmapByTime(qint64 timeMs, qint64 *realPixmapTimeMs)
{
    QMutexLocker locker(&m_mutex);
    QMap<qint64, QPixmapPtr>::iterator itrUp = m_images.lowerBound(timeMs);
    QMap<qint64, QPixmapPtr>::iterator itrDown = m_images.lowerBound(timeMs - m_step);
    QMap<qint64, QPixmapPtr>::iterator bestItr;
    if (itrUp != m_images.end() && qAbs(itrUp.key() - timeMs) <= qAbs(itrDown.key() - timeMs))
        bestItr = itrUp;
    else
        bestItr = itrDown;
    
    if (bestItr == m_images.end() || qAbs(bestItr.key() - timeMs) >= m_step) 
    {
        if (realPixmapTimeMs)
            *realPixmapTimeMs = -1;

        if (bestItr != m_images.end())
        {
            qint64 eps = qAbs(bestItr.key() - timeMs);
            eps = eps;
        }

        return QPixmapPtr();
    }
    else 
    {
        if (realPixmapTimeMs)
            *realPixmapTimeMs = bestItr.key();
        return bestItr.value();
    }
}

void QnThumbnailsLoader::ensureScaleContext(int lineSize, const QSize &size, const QSize &boundingSize, PixelFormat format)
{
    QSize dstSize = QnGeometry::expanded(QnGeometry::aspectRatio(size), boundingSize, Qt::KeepAspectRatio).toSize();
    
    if (m_scaleContext) {
        if (m_srcLineSize == lineSize && m_srcSize == size && m_srcFormat == format && m_dstSize == dstSize)
            return;

        sws_freeContext(m_scaleContext);
        m_scaleContext = 0;
    }
        
    m_srcLineSize = lineSize;
    m_srcSize = size;
    m_srcFormat = format;
    m_dstSize = dstSize;
    qFreeAligned(m_rgbaBuffer);

    int numBytes = avpicture_get_size(PIX_FMT_RGBA, qPower2Ceil(static_cast<quint32>(m_dstSize.width()), 8), m_dstSize.height());
    m_rgbaBuffer = static_cast<quint8 *>(qMallocAligned(numBytes, 32));
    m_scaleContext = sws_getContext(m_srcSize.width(), m_srcSize.height(), format, m_dstSize.width(), m_dstSize.height(), PIX_FMT_BGRA, SWS_POINT, NULL, NULL, NULL);
}

bool QnThumbnailsLoader::processFrame(const CLVideoDecoderOutput &outFrame, const QSize &boundingSize)
{
    int dstLineSize[4];
    quint8* dstBuffer[4];

    ensureScaleContext(outFrame.linesize[0], QSize(outFrame.width, outFrame.height), boundingSize, (PixelFormat) outFrame.format);

    dstLineSize[0] = qPower2Ceil(static_cast<quint32>(m_dstSize.width() * 4), 32);
    dstLineSize[1] = dstLineSize[2] = dstLineSize[3] = 0;
    QImage image(m_rgbaBuffer, m_dstSize.width(), m_dstSize.height(), dstLineSize[0], QImage::Format_ARGB32_Premultiplied);
    dstBuffer[0] = dstBuffer[1] = dstBuffer[2] = dstBuffer[3] = m_rgbaBuffer;

    sws_scale(m_scaleContext, outFrame.data, outFrame.linesize, 0, outFrame.height, dstBuffer, dstLineSize);
    
    QMutexLocker lock(&m_mutex);
    if (m_breakCurrentJob)
        return false;

    QPixmapPtr pixmap(new QPixmap(QPixmap::fromImage(image)));
    m_newImages.insert(outFrame.pkt_dts / 1000, pixmap);
    emit gotNewPixmap(outFrame.pkt_dts / 1000, pixmap);

    return true;
}

void QnThumbnailsLoader::run()
{
    while (!m_needStop)
    {
        QSize boundingSize = this->boundingSize();
        if (m_rangeToLoad.isEmpty() || boundingSize.isEmpty())
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

        QnCompressedVideoDataPtr frame = qSharedPointerDynamicCast<QnCompressedVideoData>(m_rtspClient->getNextData());
        if (!frame) 
            continue;

        CLFFmpegVideoDecoder decoder(frame->compressionType, frame, false);
        CLVideoDecoderOutput outFrame;
        outFrame.setUseExternalData(false);

        while (!m_needStop && frame)
        {
            if (decoder.decode(frame, &outFrame))
            {
                if (!processFrame(outFrame, boundingSize))
                    break;
            }

            frame = qSharedPointerDynamicCast<QnCompressedVideoData> (m_rtspClient->getNextData());
        }

        QnCompressedVideoDataPtr emptyData(new QnCompressedVideoData(1,0));
        while (!m_needStop && decoder.decode(emptyData, &outFrame))
            processFrame(outFrame, boundingSize);

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

    base_type::pleaseStop();
}

#endif