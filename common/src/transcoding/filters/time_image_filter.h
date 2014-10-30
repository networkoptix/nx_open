#ifndef __TIME_IMAGE_FILTER_H__
#define __TIME_IMAGE_FILTER_H__

#ifdef ENABLE_DATA_PROVIDERS

#include <QtGui/QFont>

#include "abstract_image_filter.h"

class QnTimeImageFilter: public QnAbstractImageFilter
{
public:
    QnTimeImageFilter(Qn::Corner datePos, qint64 timeOffsetMs);
    virtual ~QnTimeImageFilter();
    CLVideoDecoderOutputPtr updateImage(const CLVideoDecoderOutputPtr& frame, const QRectF& updateRect, qreal ar) override;

private:
    void initTimeDrawing(const CLVideoDecoderOutputPtr& frame, const QString& timeStr);

private:
    QFont m_timeFont;
    int m_dateTimeXOffs;
    int m_dateTimeYOffs;
    int m_bufXOffs;
    int m_bufYOffs;
    QImage* m_timeImg;
    qint64 m_onscreenDateOffset;
    quint8* m_imageBuffer;
    Qn::Corner m_dateTextPos;
};

#endif // ENABLE_DATA_PROVIDER

#endif // __TIME_IMAGE_FILTER_H__
