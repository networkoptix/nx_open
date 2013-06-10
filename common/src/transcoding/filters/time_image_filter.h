#ifndef __TIME_IMAGE_FILTER_H__
#define __TIME_IMAGE_FILTER_H__

#include <QFont>
#include "abstract_filter.h"

class QnTimeImageFilter: public QnAbstractImageFilter
{
public:
    enum OnScreenDatePos {Date_None, Date_LeftTop, Date_RightTop, Date_RightBottom, Date_LeftBottom};

    QnTimeImageFilter(OnScreenDatePos datePos, int timeOffsetMs);
    virtual ~QnTimeImageFilter();
    virtual void updateImage(CLVideoDecoderOutput* frame, const QRectF& updateRect) override;
private:
    void initTimeDrawing(CLVideoDecoderOutput* frame, const QString& timeStr);
private:
    QFont m_timeFont;
    int m_dateTimeXOffs;
    int m_dateTimeYOffs;
    int m_bufXOffs;
    int m_bufYOffs;
    QImage* m_timeImg;
    int m_onscreenDateOffset;
    quint8* m_imageBuffer;
    OnScreenDatePos m_dateTextPos;
};

#endif // __TIME_IMAGE_FILTER_H__
