#pragma once

#ifdef ENABLE_DATA_PROVIDERS

#include <QtGui/QFont>

#include "abstract_image_filter.h"

#include <transcoding/timestamp_params.h>

class QnResourceVideoLayout;

class QnTimeImageFilter: public QnAbstractImageFilter
{
public:
    QnTimeImageFilter(
        const QSharedPointer<const QnResourceVideoLayout>& videoLayout,
        const QnTimeStampParams& params);
    virtual ~QnTimeImageFilter();
    CLVideoDecoderOutputPtr updateImage(const CLVideoDecoderOutputPtr& frame) override;
    virtual QSize updatedResolution(const QSize& srcSize) override { return srcSize; }
private:
    void initTimeDrawing(const CLVideoDecoderOutputPtr& frame, const QString& timeStr);
    qint64 calcHash(const quint8* data, int width, int height, int linesize);
private:
    QFont m_timeFont;
    QPoint m_dateTimePos;
    int m_dateTimeXOffs;
    int m_dateTimeYOffs;
    int m_bufXOffs;
    int m_bufYOffs;
    QImage* m_timeImg;
    QSize m_timeImgSrcSiz;
    quint8* m_imageBuffer;
    bool m_checkHash;
    qint64 m_hash;
    QnTimeStampParams m_params;
};

#endif // ENABLE_DATA_PROVIDERS
