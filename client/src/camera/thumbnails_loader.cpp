#include "thumbnails_loader.h"

#include <QtGui/QPixmap>
#include <QtGui/QImage>

#include "decoders/video/ffmpeg.h"

#include "ui/common/geometry.h"


QnThumbnailsLoader::QnThumbnailsLoader(QnResourcePtr resource):
    m_mutex(QMutex::Recursive),
    m_rtspClient(new QnRtspClientArchiveDelegate()),
    m_resource(resource),
    m_step(0),
    m_startTime(0),
    m_endTime(0),
    m_scaleContext(NULL),
    m_rgbaBuffer(NULL),
    m_boundingSize(128, 128 * 3 / 4),
    m_dstSize(0, 0),
    m_srcSize(0, 0),
    m_srcLineSize(0),
    m_srcFormat(0),
    m_lastLoadedTime(0),
    m_breakCurrentJob(false)
{
    static volatile bool metaTypesInitialized = false;
    if (!metaTypesInitialized) {
        qRegisterMetaType<QPixmapPtr>();
        metaTypesInitialized = true;
    }

    start();
}

QnThumbnailsLoader::~QnThumbnailsLoader() {
    pleaseStop();
    stop();
    if (m_scaleContext)
        sws_freeContext(m_scaleContext);
    qFreeAligned(m_rgbaBuffer);
}

void QnThumbnailsLoader::setBoundingSize(const QSize &size) 
{
    QMutexLocker locker(&m_mutex);

    m_boundingSize = size;
}

QSize QnThumbnailsLoader::boundingSize() const 
{
    QMutexLocker locker(&m_mutex);

    return m_boundingSize;
}

QSize QnThumbnailsLoader::thumbnailSize() const {
    QMutexLocker locker(&m_mutex);

    return m_dstSize;
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

void QnThumbnailsLoader::ensureScaleContext(int lineSize, const QSize &size, PixelFormat format)
{
    QSize dstSize = QnGeometry::expanded(QnGeometry::aspectRatio(size), m_boundingSize, Qt::KeepAspectRatio).toSize();
    
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

bool QnThumbnailsLoader::processFrame(const CLVideoDecoderOutput &outFrame)
{
    int dstLineSize[4];
    quint8* dstBuffer[4];

    ensureScaleContext(outFrame.linesize[0], QSize(outFrame.width, outFrame.height), (PixelFormat) outFrame.format);

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
        if (m_rangeToLoad.isEmpty() || m_boundingSize.isEmpty())
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

    base_type::pleaseStop();
}

qint64 QnThumbnailsLoader::step() const
{
    return m_step;
}
