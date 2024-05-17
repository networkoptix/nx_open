// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtGui/QFont>

#include <core/resource/resource_media_layout.h>

#include "abstract_image_filter.h"

class NX_VMS_COMMON_API QnTiledImageFilter: public QnAbstractImageFilter
{
public:
    explicit QnTiledImageFilter(const QnConstResourceVideoLayoutPtr& videoLayout);
    virtual CLVideoDecoderOutputPtr updateImage(
        const CLVideoDecoderOutputPtr& frame,
        const QnAbstractCompressedMetadataPtr& metadata) override;

    virtual QSize updatedResolution(const QSize& srcSize) override;

private:
    QnConstResourceVideoLayoutPtr m_layout;
    CLVideoDecoderOutputPtr m_tiledFrame;
    QSize m_size;
    qint64 m_prevFrameTime = 0;
};
