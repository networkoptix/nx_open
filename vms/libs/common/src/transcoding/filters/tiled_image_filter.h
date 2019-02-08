#pragma once

#if defined(ENABLE_DATA_PROVIDERS)

#include <QtGui/QFont>

#include "abstract_image_filter.h"

#include <core/resource/resource_media_layout.h>

class QnTiledImageFilter: public QnAbstractImageFilter
{
public:
    explicit QnTiledImageFilter(const QnConstResourceVideoLayoutPtr& videoLayout);
    virtual CLVideoDecoderOutputPtr updateImage(const CLVideoDecoderOutputPtr& frame) override;
    virtual QSize updatedResolution(const QSize& srcSize) override;

private:
    QnConstResourceVideoLayoutPtr m_layout;
    CLVideoDecoderOutputPtr m_tiledFrame;
    QSize m_size;
    qint64 m_prevFrameTime = 0;
};

#endif // ENABLE_DATA_PROVIDER
