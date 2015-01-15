#include "thumbnails_loader.h"

#include <cassert>
#include <limits>

extern "C"
{
    #include <libswscale/swscale.h>
}

#include <QtCore/QTimer>
#include <QtGui/QImage>

#include <utils/math/math.h>
#include <utils/common/synctime.h>
#include <utils/common/performance.h>

#include "core/resource/camera_resource.h"
#include "core/resource/camera_history.h"

#include "decoders/video/ffmpeg.h"

#include "ui/common/geometry.h"

#include "plugins/resource/avi/thumbnails_archive_delegate.h"
#include "plugins/resource/archive/archive_stream_reader.h"

#include "plugins/resource/archive/rtsp_client_archive_delegate.h"

#include "utils/media/frame_info.h"

#include "thumbnails_loader_helper.h"
#include "plugins/resource/avi/avi_resource.h"

#include <recording/time_period.h>

namespace {
    const qint64 defaultUpdateInterval = 10 * 1000; /* 10 seconds. */

    const int maxStackSize = 1024;

    const int maxThumbnailCacheSize = 20 * 1024 * 1024; /* 20 megabytes per loader. */

    const qint64 invalidProcessingTime = std::numeric_limits<qint64>::min() / 2;
}

//#define QN_THUMBNAILS_LOADER_DEBUG

QnThumbnailsLoader::QnThumbnailsLoader(const QnResourcePtr &resource, QnThumbnailsLoader::Mode mode):
    m_mutex(QMutex::NonRecursive),
    m_resource(resource),
    m_mode(mode),
    m_timeStep(0),
    m_requestStart(0),
    m_requestEnd(0),
    m_processingStart(invalidProcessingTime),
    m_processingEnd(invalidProcessingTime),
    m_boundingSize(128, 96), /* That's 4:3 aspect ratio. */
    m_scaleContext(NULL),
    m_scaleBuffer(NULL),
    m_scaleSourceSize(0, 0),
    m_scaleTargetSize(0, 0),
    m_scaleSourceLine(0),
    m_scaleSourceFormat(0),
    m_helper(NULL),
    m_generation(0),
    m_cachedAspectRatio(0.0)
{
    connect(this, SIGNAL(updateProcessingLater()), this, SLOT(updateProcessing()), Qt::QueuedConnection);

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

QSize QnThumbnailsLoader::sourceSize() const {
    QMutexLocker locker(&m_mutex);

    return m_scaleSourceSize;
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

    emit updateProcessingLater(); /* We don't need to unlock here. */
}

QnTimePeriod QnThumbnailsLoader::timePeriod() const {
    QMutexLocker locker(&m_mutex);

    return QnTimePeriod(m_requestStart, m_requestEnd - m_requestStart);
}

QHash<qint64, QnThumbnail> QnThumbnailsLoader::thumbnails() const {
    QMutexLocker locker(&m_mutex);

    return m_thumbnailByTime;
}

void QnThumbnailsLoader::pleaseStop() {
    {
        QMutexLocker locker(&m_mutex);

        foreach(QnAbstractArchiveDelegatePtr client, m_delegates) 
            client->beforeClose();
    }

    quit();
    base_type::pleaseStop();
}

qreal QnThumbnailsLoader::getAspectRatio()
{
    // do not change aspect ratio for resource during loading data. If aspect ratio is changed (for difference points of archive), QnThumbnailsLoader goes to infinity loop!
    // todo: need update aspect ratio in future

    if (m_scaleSourceSize.isEmpty())
        return QnGeometry::aspectRatio(m_scaleSourceSize);
    if (m_cachedAspectRatio == 0) 
        m_cachedAspectRatio = QnGeometry::aspectRatio(m_scaleSourceSize);
    return m_cachedAspectRatio;
}

void QnThumbnailsLoader::updateTargetSizeLocked(bool invalidate) {
    QSize targetSize;

    if(!m_scaleSourceSize.isEmpty() && !m_boundingSize.isEmpty())
        targetSize = QnGeometry::expanded(getAspectRatio(), m_boundingSize, Qt::KeepAspectRatio).toSize();

    if(m_targetSize == targetSize)
        return;

    m_targetSize = targetSize;

    if(invalidate)
        invalidateThumbnailsLocked();
}

void QnThumbnailsLoader::invalidateThumbnailsLocked() {
    m_thumbnailByTime.clear();
    m_processingStack.clear();
    m_maxLoadedTime = 0;
    m_processingStart = m_processingEnd = invalidProcessingTime;
    m_generation++;

    m_mutex.unlock();
    emit updateProcessingLater();
    emit thumbnailsInvalidated();
    m_mutex.lock();
}

void QnThumbnailsLoader::updateProcessing() {
    QMutexLocker locker(&m_mutex);

    updateProcessingLocked();
}

void QnThumbnailsLoader::updateProcessingLocked() {
    /* Don't load anything if request fits into processed period. */
    if(m_requestStart >= m_processingStart && m_requestEnd <= m_processingEnd)
        return;

    /* Also don't load anything if this loader is not yet initialized. */
    if(m_timeStep == 0 || m_boundingSize.isEmpty() || (m_requestStart == 0 && m_requestEnd == 0))
        return;

    bool processingPeriodValid = m_processingStart >= 0 && m_processingEnd >= 0;

    /* Discard old loaded period if it cannot be used to extend the new one. */
    if(processingPeriodValid && (m_requestStart > m_processingEnd + m_timeStep || m_requestEnd < m_processingStart - m_timeStep)) {
        invalidateThumbnailsLocked();
        return; /* We'll be called again from the event loop. */
    }

    // TODO: #Elric there probably is a better place for checking size restrictions.
    if(m_thumbnailByTime.size() * m_scaleTargetSize.width() * m_scaleTargetSize.width() * 4 > maxThumbnailCacheSize) {
        invalidateThumbnailsLocked();
        return; /* We'll be called again from the event loop. */
    }

    qint64 processingStart = m_requestStart;
    qint64 processingEnd = m_requestEnd;
    qint64 processingSize = processingEnd - processingStart;

    if (m_mode == Mode::Default) {
        /* Add margins, shifting thumbnail time to a quarter of the period. */
        processingStart = qFloor(qMax(0ll, processingStart - processingSize / 4), m_timeStep);
        processingEnd = qCeil(processingEnd + processingSize / 4, m_timeStep);
    } else {
        /* Add only left margin to make sure first thumbnail is loaded. */
        processingStart = qMax(0ll, processingStart - m_timeStep);
    }

    /* Trim at live. */
    qint64 currentTime = qnSyncTime->currentMSecsSinceEpoch();
    if(processingEnd > currentTime)
        processingEnd = qCeil(currentTime + defaultUpdateInterval, m_timeStep);

    /* Adjust for the chunks near live that could not be loaded at the last request. */
    if(m_processingStart < m_maxLoadedTime && m_maxLoadedTime < m_processingEnd && processingEnd > m_maxLoadedTime) {
        processingStart = qMin(processingStart, qFloor(m_maxLoadedTime, m_timeStep) + m_timeStep);
        m_processingEnd = m_maxLoadedTime;
    }

    if(processingPeriodValid) {
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

        m_processingStart = qMin(m_processingStart, processingStart);
        m_processingEnd = qMax(m_processingEnd, processingEnd);
    } else {
        enqueueForProcessingLocked(processingStart, processingEnd);

        m_processingStart = processingStart;
        m_processingEnd = processingEnd;
    }
}

void QnThumbnailsLoader::enqueueForProcessingLocked(qint64 startTime, qint64 endTime) {
    m_processingStack.push(QnTimePeriod(startTime, endTime - startTime));

#ifdef QN_THUMBNAILS_LOADER_DEBUG
    qDebug() << "QnThumbnailsLoader::enqueueForProcessingLocked [" << startTime << "," << endTime << ") STEP" << m_timeStep;
#endif

    while(m_processingStack.size() > maxStackSize)
        m_processingStack.pop();

    // if(m_processingStack.size() == 1) // TODO: #Elric for some reason, this doesn't work in some cases.
        emit processingRequested();
}

void QnThumbnailsLoader::run() {
    assert(QThread::currentThread() == this); /* This function is supposed to be run from thumbnail rendering thread only. */

    m_helper = new QnThumbnailsLoaderHelper(this);
    connect(this, SIGNAL(processingRequested()), m_helper, SLOT(process()), Qt::QueuedConnection);
    connect(m_helper, SIGNAL(thumbnailLoaded(const QnThumbnail &)), this, SLOT(addThumbnail(const QnThumbnail &)));

    if(!m_processingStack.empty())
        emit processingRequested();

    base_type::run();

    delete m_helper;
    m_helper = NULL;
}

void QnThumbnailsLoader::process() {
    QnTimePeriod period;
    QSize boundingSize;
    QSize thumbnailSize;
    qint64 timeStep;
    int generation;
    QList<QnAbstractArchiveDelegatePtr> delegates;

    {
        QMutexLocker locker(&m_mutex);

        if(m_processingStack.isEmpty())
            return;

        period = m_processingStack.pop();
        boundingSize = m_boundingSize;
        thumbnailSize = m_scaleTargetSize;
        timeStep = m_timeStep;
        generation = m_generation;
    }

#ifdef QN_THUMBNAILS_LOADER_DEBUG
    qDebug() << "QnThumbnailsLoader::process START [" << period.startTimeMs << "," << period.endTimeMs() + timeStep << ")";
    qint64 startRealTime = QDateTime::currentMSecsSinceEpoch();
    qint64 startCpuTime = QnPerformance::currentThreadTimeMSecs();
#endif

    QnVirtualCameraResourcePtr camera = qSharedPointerDynamicCast<QnVirtualCameraResource>(m_resource);
    if (camera) {
        QnMediaServerResourceList servers = QnCameraHistoryPool::instance()->getOnlineCameraServers(camera, period);
        for (int i = 0; i < servers.size(); ++i) 
        {
            QnRtspClientArchiveDelegatePtr rtspDelegate(new QnRtspClientArchiveDelegate(0));
            rtspDelegate->setMultiserverAllowed(false);
            if (m_mode == Mode::Default)
                rtspDelegate->setQuality(MEDIA_Quality_Low, true);
            else
                rtspDelegate->setQuality(MEDIA_Quality_High, true);
            QnThumbnailsArchiveDelegatePtr thumbnailDelegate(new QnThumbnailsArchiveDelegate(rtspDelegate));
            rtspDelegate->setCamera(camera);
            rtspDelegate->setFixedServer(servers[i]);
            delegates << thumbnailDelegate;
        }
    }
    else {
        QnAviArchiveDelegatePtr aviDelegate;

        QnAviResourcePtr aviFile = qSharedPointerDynamicCast<QnAviResource>(m_resource);
        if (aviFile)
            aviDelegate = QnAviArchiveDelegatePtr(aviFile->createArchiveDelegate());
        else
            aviDelegate = QnAviArchiveDelegatePtr(new QnAviArchiveDelegate);

        QnThumbnailsArchiveDelegatePtr thumbnailDelegate(new QnThumbnailsArchiveDelegate(aviDelegate));
        if (thumbnailDelegate->open(m_resource))
            delegates << thumbnailDelegate;
    }

    m_mutex.lock();
    m_delegates = delegates;
    m_mutex.unlock();


    bool invalidated = false;
    foreach(QnAbstractArchiveDelegatePtr client, delegates) 
    {
        client->setRange(period.startTimeMs * 1000, period.endTimeMs() * 1000, timeStep * 1000);
        QQueue<qint64> timingsQueue;
        QQueue<int> frameFlags;

        QnThumbnail thumbnail(QImage(), thumbnailSize, period.startTimeMs, period.startTimeMs, timeStep, generation);
        qint64 time = period.startTimeMs;
        QnCompressedVideoDataPtr frame = client->getNextData().dynamicCast<QnCompressedVideoData>();
        if (frame)
        {
            CLFFmpegVideoDecoder decoder(frame->compressionType, frame, false);
            QSharedPointer<CLVideoDecoderOutput> outFrame( new CLVideoDecoderOutput() );
            outFrame->setUseExternalData(false);

            while (frame) {
                if (!m_resource->hasFlags(Qn::utc))
                    frame->flags &= ~QnAbstractMediaData::MediaFlags_BOF;

                timingsQueue << frame->timestamp;
                frameFlags << frame->flags;

                if(m_mode == Mode::Default) {
                    if (decoder.decode(frame, &outFrame)) 
                    {
                        outFrame->pkt_dts = timingsQueue.dequeue();
                        thumbnail = generateThumbnail(*outFrame, boundingSize, timeStep, generation);
                        time = processThumbnail(thumbnail, time, thumbnail.time(), frameFlags.dequeue() & QnAbstractMediaData::MediaFlags_BOF);
                    }
                } else {
                    frame->flags |= QnAbstractMediaData::MediaFlags_DecodeTwice | QnAbstractMediaData::MediaFlags_StillImage;

                    qint64 actualTime = (timingsQueue.dequeue() + 999) / 1000;
                    thumbnail = QnThumbnail(frame, qRound(actualTime, timeStep), actualTime, timeStep, generation);
                    time = processThumbnail(thumbnail, time, thumbnail.time(), frameFlags.dequeue() & QnAbstractMediaData::MediaFlags_BOF);
                }

                {
                    QMutexLocker locker(&m_mutex);
                    if(m_generation != generation) {
                        invalidated = true;
                        break;
                    }
                }

                frame = qSharedPointerDynamicCast<QnCompressedVideoData>(client->getNextData());
            }

            if(!invalidated && m_mode == Mode::Default) { // TODO: #Elric mode check may be wrong here.
                /* Make sure decoder's buffer is empty. */
                QnWritableCompressedVideoDataPtr emptyData(new QnWritableCompressedVideoData(1, 0));
                while (decoder.decode(emptyData, &outFrame)) 
                {
                    if(timingsQueue.isEmpty()) {
                        qnWarning("Time queue was emptied unexpectedly.");
                        break;
                    }

                    outFrame->pkt_dts = timingsQueue.dequeue();
                    thumbnail = generateThumbnail(*outFrame, boundingSize, timeStep, generation);
                    time = processThumbnail(thumbnail, time, thumbnail.time(), frameFlags.dequeue() & QnAbstractMediaData::MediaFlags_BOF);
                }
            }
        }
        client->close();

        if(invalidated)
            break;
    }

#ifdef QN_THUMBNAILS_LOADER_DEBUG
    qint64 totalRealTime = QDateTime::currentMSecsSinceEpoch() - startRealTime;
    qint64 totalCpuTime = QnPerformance::currentThreadTimeMSecs() - startCpuTime;
    qDebug() << "QnThumbnailsLoader::process END [" << period.startTimeMs << "," << period.endTimeMs() + timeStep << ") IN " << totalCpuTime << "/" << totalRealTime << "ms cpu/real time";
#endif

    /* Go on with processing. */
    QMetaObject::invokeMethod(m_helper, "process", Qt::QueuedConnection); // TODO: #Elric use connections.
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
    Q_UNUSED(boundingSize)
    bool needsReallocation = false;
    
    if(m_scaleSourceSize != sourceSize) {
        m_scaleSourceSize = sourceSize;
        m_scaleSourceFormat = format;
        m_scaleSourceLine = lineSize;
        updateTargetSizeLocked(false);
        
        m_mutex.unlock();
        emit sourceSizeChanged();
        m_mutex.lock();

        needsReallocation = true;
    }

    if(m_scaleSourceLine != lineSize || m_scaleSourceSize != sourceSize || m_scaleSourceFormat != format || m_targetSize != m_scaleTargetSize)
        needsReallocation = true;

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
        m_scaleContext = sws_getContext(m_scaleSourceSize.width(), m_scaleSourceSize.height(), static_cast<PixelFormat>(m_scaleSourceFormat), m_scaleTargetSize.width(), m_scaleTargetSize.height(), PIX_FMT_BGRA, SWS_BICUBIC, NULL, NULL, NULL);
        // TODO: #Elric sws_getContext may fail and return NULL.
    }
}

QnThumbnail QnThumbnailsLoader::generateThumbnail(const CLVideoDecoderOutput &outFrame, const QSize &boundingSize, qint64 timeStep, int generation) {
    QSize scaleTargetSize;
    {
        QMutexLocker locker(&m_mutex);
        ensureScaleContextLocked(outFrame.linesize[0], QSize(outFrame.width*outFrame.sample_aspect_ratio, outFrame.height), boundingSize, (PixelFormat) outFrame.format);
        scaleTargetSize = m_scaleTargetSize;
    }

    int dstLineSize[4];
    quint8* dstBuffer[4];
    dstLineSize[0] = qPower2Ceil(static_cast<quint32>(scaleTargetSize.width() * 4), 32);
    dstLineSize[1] = dstLineSize[2] = dstLineSize[3] = 0;
    QImage image(m_scaleBuffer, scaleTargetSize.width(), scaleTargetSize.height(), dstLineSize[0], QImage::Format_ARGB32_Premultiplied);
    dstBuffer[0] = dstBuffer[1] = dstBuffer[2] = dstBuffer[3] = m_scaleBuffer;

    sws_scale(m_scaleContext, outFrame.data, outFrame.linesize, 0, outFrame.height, dstBuffer, dstLineSize);

    /* We need to do copy the image since it doesn't own its data buffer. */
    image = image.copy();

    qint64 actualTime = (outFrame.pkt_dts + 999) / 1000;
    return QnThumbnail(image, image.size(), qRound(actualTime, timeStep), actualTime, timeStep, generation);
}

qint64 QnThumbnailsLoader::processThumbnail(const QnThumbnail &thumbnail, qint64 startTime, qint64 endTime, bool ignorePeriod) {
    if(ignorePeriod) {
        emit m_helper->thumbnailLoaded(thumbnail);
        return qCeil(thumbnail.actualTime(), thumbnail.timeStep());
    } else {      
        for(qint64 time = startTime; time <= endTime; time += thumbnail.timeStep())
            emit m_helper->thumbnailLoaded(QnThumbnail(thumbnail, time));
        return qMax(startTime, endTime + thumbnail.timeStep());
    }
}
