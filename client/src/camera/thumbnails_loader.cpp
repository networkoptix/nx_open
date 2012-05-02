#include "thumbnails_loader.h"
#include "decoders/video/ffmpeg.h"

// ------------------------ QnThumbnailsLoader -------------------------

QnThumbnailsLoader::QnThumbnailsLoader(QnResourcePtr resource)
{
    m_resource = resource;
    m_step = 0;
    m_startTime = 0;
    m_endTime = 0;
    m_outWidth = 128;
    m_outHeight = 128*3/4;
    m_scaleContext = 0;
    m_rgbaBuffer = 0;

    m_srcLineSize = 0;
    m_srcWidth = 0;
    m_srcHeight = 0;
    m_srcFormat = 0;

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

void QnThumbnailsLoader::loadRange(qint64 startTimeUsec, qint64 endTimeUsec, qint64 stepUsec)
{
    //pleaseStop();
    //stop();
    QMutexLocker locker(&m_mutex);
    m_images.clear();
    m_startTime = startTimeUsec;
    m_endTime = endTimeUsec;
    m_step = stepUsec;

    m_rangeToLoad.clear();
    m_rangeToLoad << QnTimePeriod(startTimeUsec, endTimeUsec-startTimeUsec);
}

void QnThumbnailsLoader::addRange(qint64 startTimeUsec, qint64 endTimeUsec)
{
    QMutexLocker locker(&m_mutex);
    if (startTimeUsec < m_startTime)
    {
        m_rangeToLoad << QnTimePeriod(startTimeUsec, m_startTime - startTimeUsec);
        m_startTime = startTimeUsec;
    }
    if (endTimeUsec > m_endTime)
    {
        m_rangeToLoad << QnTimePeriod(endTimeUsec, endTimeUsec - m_endTime);
        m_endTime = endTimeUsec;
    }

}

void QnThumbnailsLoader::removeRange(qint64 startTimeUsec, qint64 endTimeUsec)
{
    QMutexLocker locker(&m_mutex);
    QMap<qint64, QPixmap>::iterator itr = m_images.lowerBound(startTimeUsec);
    QMap<qint64, QPixmap>::iterator endPos = m_images.upperBound(endTimeUsec);
    while (itr != endPos)
        itr = m_images.erase(itr);
}

QPixmap QnThumbnailsLoader::getPixmapByTime(qint64 timeUsec, quint64* realPixmapTimeUsec)
{
    QMutexLocker locker(&m_mutex);
    QMap<qint64, QPixmap>::iterator itr = m_images.lowerBound(timeUsec);
    if (realPixmapTimeUsec)
        *realPixmapTimeUsec = itr != m_images.end() ? itr.key() : -1;
    return itr != m_images.end() ? itr.value() : QPixmap();
}

void QnThumbnailsLoader::allocateScaleContext(int lineSize, int width, int height, PixelFormat format)
{
    if (m_scaleContext) 
    {
        if (m_srcLineSize == lineSize && m_srcWidth == width && m_srcHeight == height && m_srcFormat == format)
            return;

        sws_freeContext(m_scaleContext);
        m_scaleContext = 0;
        m_srcLineSize = lineSize;
        m_srcWidth = width;
        m_srcHeight = height;
        m_srcFormat = format;
    }
    qFreeAligned(m_rgbaBuffer);

    int numBytes = avpicture_get_size(PIX_FMT_RGBA, lineSize, height);
    m_rgbaBuffer = (quint8*) qMallocAligned(numBytes, 32);
    m_scaleContext = sws_getContext(width, height, format, 
        m_outWidth, m_outHeight, PIX_FMT_BGRA, SWS_POINT, NULL, NULL, NULL);
}

void QnThumbnailsLoader::run()
{
    m_rtspClient->setAdditionalAttribute("x-media-step", QByteArray::number(m_step));

    while (!m_needStop)
    {
        if (m_rangeToLoad.isEmpty())
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
        m_mutex.unlock();

        m_rtspClient->seek(period.startTimeMs, period.endTimeMs());

        QnCompressedVideoDataPtr frame = qSharedPointerDynamicCast<QnCompressedVideoData> (m_rtspClient->getNextData());
        if (!frame) 
            continue;

        CLFFmpegVideoDecoder decoder(frame->compressionType, frame, false);
        CLVideoDecoderOutput outFrame;
        outFrame.setUseExternalData(false);

        allocateScaleContext(outFrame.linesize[0], outFrame.width, outFrame.height, (PixelFormat) outFrame.format);

        int dstLineSize[4];
        quint8* dstBuffer[4];
        dstLineSize[0] = outFrame.linesize[0]*4;
        dstLineSize[1] = dstLineSize[2] = dstLineSize[3] = 0;
        QImage image(m_rgbaBuffer, outFrame.width, outFrame.height, outFrame.linesize[0]*4, QImage::Format_ARGB32_Premultiplied);
        dstBuffer[0] = dstBuffer[1] = dstBuffer[2] = dstBuffer[3] = m_rgbaBuffer;

        while (!m_needStop && frame)
        {
            if (decoder.decode(frame, &outFrame))
            {
                sws_scale(m_scaleContext,
                    outFrame.data, outFrame.linesize, 
                    0, outFrame.height, 
                    dstBuffer, dstLineSize);
                QMutexLocker lock(&m_mutex);
                QPixmap pixmap = QPixmap::fromImage(image);
                m_images.insert(frame->timestamp, pixmap);
                emit gotNewPixmap(frame->timestamp, pixmap);
            }

            frame = qSharedPointerDynamicCast<QnCompressedVideoData> (m_rtspClient->getNextData());
        }

        m_rtspClient->close();
    }
}

void QnThumbnailsLoader::pleaseStop()
{
    m_rtspClient->beforeClose();
    m_rtspClient->close();
}
