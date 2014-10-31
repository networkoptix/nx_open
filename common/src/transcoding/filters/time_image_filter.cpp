#include "time_image_filter.h"

#ifdef ENABLE_DATA_PROVIDERS

#include <QtCore/QDateTime>
#include <QtGui/QFontMetrics>
#include <QtGui/QImage>
#include <QtGui/QPainter>

#include <utils/common/util.h> /* For UTC_TIME_DETECTION_THRESHOLD. */
#include <utils/math/math.h>
#include <utils/media/frame_info.h>
#include <utils/color_space/yuvconvert.h>
#include "core/resource/resource_media_layout.h"


static const int TEXT_HEIGHT_IN_FRAME_PARTS = 25;
static const int MIN_TEXT_HEIGHT = 14;
static const double FPS_EPS = 1e-8;

QnTimeImageFilter::QnTimeImageFilter(const QSharedPointer<const QnResourceVideoLayout>& videoLayout, Qn::Corner datePos, qint64 timeOffsetMs):
    m_dateTimeXOffs(0),
    m_dateTimeYOffs(0),
    m_bufXOffs(0),
    m_bufYOffs(0),
    m_timeImg(0),
    m_onscreenDateOffset(timeOffsetMs),
    m_imageBuffer(0),
    m_dateTextPos(datePos),
    m_channel(0)
{
    QPoint textPos;
    switch(m_dateTextPos)
    {
    case Qn::TopLeftCorner:
        textPos = QPoint(0, 0);
        break;
    case Qn::TopRightCorner:
        textPos = QPoint(videoLayout->size().width()-1, 0);
        break;
    case Qn::BottomRightCorner:
        textPos = QPoint(videoLayout->size().width()-1, videoLayout->size().height()-1);
        break;
    case Qn::BottomLeftCorner:
    default:
        textPos = QPoint(0, videoLayout->size().height()-1);
        break;
    }
    for (int ch = 0; ch < videoLayout->channelCount(); ++ch) {
        if (videoLayout->position(ch) == textPos)
            m_channel = ch;
    }
}

QnTimeImageFilter::~QnTimeImageFilter()
{
    delete m_timeImg;
    qFreeAligned(m_imageBuffer);
}

void QnTimeImageFilter::initTimeDrawing(const CLVideoDecoderOutputPtr& frame, const QString& timeStr)
{
    m_timeFont.setBold(true);
    m_timeFont.setPixelSize(qMax(MIN_TEXT_HEIGHT, frame->height / TEXT_HEIGHT_IN_FRAME_PARTS));
    QFontMetrics metric(m_timeFont);
    
    while (metric.width(timeStr) >= frame->width && m_timeFont.pixelSize() > 0)
    {
        m_timeFont.setPixelSize(m_timeFont.pixelSize()-1);
        metric = QFontMetrics(m_timeFont);
    }

    switch(m_dateTextPos)
    {
    case Qn::TopLeftCorner:
        m_bufYOffs = 0;
        m_dateTimeXOffs = metric.averageCharWidth()/2;
        break;
    case Qn::TopRightCorner:
        m_bufYOffs = 0;
        m_dateTimeXOffs = frame->width - metric.width(timeStr) - metric.averageCharWidth()/2;
        break;
    case Qn::BottomRightCorner:
        m_bufYOffs = frame->height - metric.height();
        m_dateTimeXOffs = frame->width - metric.boundingRect(timeStr).width() - metric.averageCharWidth()/2; // - metric.width(QLatin1String("0"));
        break;
    case Qn::BottomLeftCorner:
    default:
        m_bufYOffs = frame->height - metric.height();
        m_dateTimeXOffs = metric.averageCharWidth()/2;
        break;
    }

    m_bufYOffs = qPower2Floor(m_bufYOffs, 2);
    m_bufXOffs = qPower2Floor(m_dateTimeXOffs, CL_MEDIA_ALIGNMENT);

    m_dateTimeXOffs = m_dateTimeXOffs%CL_MEDIA_ALIGNMENT;
    m_dateTimeYOffs = metric.ascent();

    int drawWidth = metric.width(timeStr);
    int drawHeight = metric.height();
    drawWidth = qPower2Ceil((unsigned) drawWidth + m_dateTimeXOffs, CL_MEDIA_ALIGNMENT);
    m_imageBuffer = (uchar*) qMallocAligned(drawWidth * drawHeight * 4, CL_MEDIA_ALIGNMENT);
    m_timeImg = new QImage(m_imageBuffer, drawWidth, drawHeight, drawWidth*4, QImage::Format_ARGB32_Premultiplied);
}

CLVideoDecoderOutputPtr QnTimeImageFilter::updateImage(const CLVideoDecoderOutputPtr& frame)
{
    if (frame->channel != m_channel)
        return frame;

    QString timeStr;
    qint64 displayTime = frame->pts/1000 + m_onscreenDateOffset;
    if (frame->pts >= UTC_TIME_DETECTION_THRESHOLD)
        timeStr = QDateTime::fromMSecsSinceEpoch(displayTime).toString(QLatin1String("yyyy-MMM-dd hh:mm:ss"));
    else
        timeStr = QTime().addMSecs(displayTime).toString(QLatin1String("hh:mm:ss.zzz"));

    if (m_timeImg == 0)
        initTimeDrawing(frame, timeStr);

    int bufPlaneYOffs  = m_bufXOffs + m_bufYOffs * frame->linesize[0];
    int bufferUVOffs = m_bufXOffs/2 + m_bufYOffs * frame->linesize[1] / 2;

    // copy and convert frame buffer to image
    yuv420_argb32_simd_intr(m_imageBuffer,
        frame->data[0]+bufPlaneYOffs, frame->data[1]+bufferUVOffs, frame->data[2]+bufferUVOffs,
        m_timeImg->width(), m_timeImg->height(),
        m_timeImg->bytesPerLine(), 
        frame->linesize[0], frame->linesize[1], 255);

    QPainter p(m_timeImg);
    p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    QPainterPath path;
    path.addText(m_dateTimeXOffs, m_dateTimeYOffs, m_timeFont, timeStr);
    p.setBrush(Qt::white);
    p.drawPath(path);
    p.strokePath(path, QPen(QColor(32,32,32,80)));

    // copy and convert RGBA32 image back to frame buffer
    bgra_to_yv12_simd_intr(m_imageBuffer, m_timeImg->bytesPerLine(), 
        frame->data[0]+bufPlaneYOffs, frame->data[1]+bufferUVOffs, frame->data[2]+bufferUVOffs,
        frame->linesize[0], frame->linesize[1], 
        m_timeImg->width(), m_timeImg->height(), false);
    return frame;
}

#endif // ENABLE_DATA_PROVIDERS
