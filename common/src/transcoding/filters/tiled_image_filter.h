#ifndef __TILED_IMAGE_FILTER_H__
#define __TILED_IMAGE_FILTER_H__

#ifdef ENABLE_DATA_PROVIDERS

#include <QtGui/QFont>

#include "abstract_image_filter.h"

class QnResourceVideoLayout;

class QnTiledImageFilter: public QnAbstractImageFilter
{
public:
    QnTiledImageFilter(const QSharedPointer<const QnResourceVideoLayout>& videoLayout);
    virtual CLVideoDecoderOutputPtr updateImage(const CLVideoDecoderOutputPtr& frame) override;
    virtual QSize updatedResolution(const QSize& srcSize) override;
private:
    QSharedPointer<const QnResourceVideoLayout> m_layout;
    CLVideoDecoderOutputPtr m_tiledFrame;
    QSize m_size;
    qint64 m_prevFrameTime; 
};

#endif // ENABLE_DATA_PROVIDER

#endif // __TILED_IMAGE_FILTER_H__
