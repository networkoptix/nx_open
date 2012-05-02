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
}

QnThumbnailsLoader::~QnThumbnailsLoader()
{

}

void QnThumbnailsLoader::setThumbnailsSize(int width, int height)
{
    m_outWidth = width;
    m_outHeight = height;
}

void QnThumbnailsLoader::loadRange(qint64 startTime, qint64 endTime, qint64 step)
{
    pleaseStop();
    stop();
    m_images.clear();
    m_startTime = startTime;
    m_endTime = endTime;
    m_step = step;
    start();
}

void QnThumbnailsLoader::addRange(qint64 startTime, qint64 endTime)
{

}

void QnThumbnailsLoader::removeRange(qint64 startTime, qint64 endTime)
{

}

QPixmap QnThumbnailsLoader::getPixmapByTime(qint64 time)
{
    return QPixmap();
}

void QnThumbnailsLoader::run()
{
    m_rtspClient->open(m_resource);
    m_rtspClient->setAdditionalAttribute("x-media-step", QByteArray::number(m_step));
    m_rtspClient->seek(m_startTime, m_endTime);

    QnCompressedVideoDataPtr frame = qSharedPointerDynamicCast<QnCompressedVideoData> (m_rtspClient->getNextData());
    if (!frame) {
        pleaseStop();
        return;
    }

    CLFFmpegVideoDecoder decoder(frame->compressionType, frame, false);
    CLVideoDecoderOutput outFrame;
    outFrame.setUseExternalData(false);

    int numBytes = avpicture_get_size(PIX_FMT_RGBA, outFrame.linesize[0], outFrame.height);
    quint8* rgbaBuffer = (quint8*) qMallocAligned(numBytes, 32);
    SwsContext* scaleContext = sws_getContext(outFrame.width, outFrame.height, (PixelFormat) outFrame.format, 
                                              m_outWidth, m_outHeight, PIX_FMT_BGRA, SWS_POINT, NULL, NULL, NULL);

    int dstLineSize[4];
    quint8* dstBuffer[4];
    dstLineSize[0] = outFrame.linesize[0]*4;
    dstLineSize[1] = dstLineSize[2] = dstLineSize[3] = 0;
    QImage image(rgbaBuffer, outFrame.width, outFrame.height, outFrame.linesize[0]*4, QImage::Format_ARGB32_Premultiplied);
    dstBuffer[0] = dstBuffer[1] = dstBuffer[2] = dstBuffer[3] = rgbaBuffer;

    while (!m_needStop && frame)
    {
        if (decoder.decode(frame, &outFrame))
        {
            sws_scale(scaleContext,
                outFrame.data, outFrame.linesize, 
                0, outFrame.height, 
                dstBuffer, dstLineSize);

            m_images.insert(frame->timestamp, QPixmap::fromImage(image));
        }

        frame = qSharedPointerDynamicCast<QnCompressedVideoData> (m_rtspClient->getNextData());
    }

    qFreeAligned(rgbaBuffer);
    sws_freeContext(scaleContext);
}

void QnThumbnailsLoader::pleaseStop()
{
    m_rtspClient->beforeClose();
    m_rtspClient->close();
}
