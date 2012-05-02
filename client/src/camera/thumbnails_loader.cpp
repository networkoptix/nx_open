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
}

QnThumbnailsLoader::~QnThumbnailsLoader()
{
    if (m_scaleContext)
        sws_freeContext(m_scaleContext);
    delete [] m_rgbaBuffer;
}

void QnThumbnailsLoader::setThumbnailsSize(int width, int height)
{
    m_outWidth = width;
    m_outHeight = height;
}

void QnThumbnailsLoader::loadRange(qint64 startTime, qint64 endTime, qint64 step)
{
    //pleaseStop();
    //stop();
    m_images.clear();
    m_startTime = startTime;
    m_endTime = endTime;
    m_step = step;

    m_rangeToLoad.clear();
    m_rangeToLoad << QnTimePeriod(startTime, endTime-startTime);
    start();
}

void QnThumbnailsLoader::addRange(qint64 startTime, qint64 endTime)
{
    if (startTime < m_startTime)
    {
        m_rangeToLoad << QnTimePeriod(startTime, m_startTime - startTime);
        m_startTime = startTime;
    }
    if (endTime > m_endTime)
    {
        m_rangeToLoad << QnTimePeriod(endTime, endTime - m_endTime);
        m_endTime = endTime;
    }

}

void QnThumbnailsLoader::removeRange(qint64 startTime, qint64 endTime)
{

}

QPixmap QnThumbnailsLoader::getPixmapByTime(qint64 time)
{
    return QPixmap();
}

void QnThumbnailsLoader::allocateScaleContext(int lineSize, int width, int height, PixelFormat format)
{
    if (m_scaleContext) {
        sws_freeContext(m_scaleContext);
        m_scaleContext = 0;
    }
    delete [] m_rgbaBuffer;

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
        m_rtspClient->open(m_resource);

        
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

                m_images.insert(frame->timestamp, QPixmap::fromImage(image));
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
