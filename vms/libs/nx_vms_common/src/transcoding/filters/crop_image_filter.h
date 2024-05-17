// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>

#include "abstract_image_filter.h"

class NX_VMS_COMMON_API QnCropImageFilter: public QnAbstractImageFilter
{
public:
    /**
     * Construct crop filter with fixed size
     */
    QnCropImageFilter(const QRect& rect, bool alignSize = true);

    /**
     * Construct crop filter with dynamic size, measured in parts [0..1] of a source frame size
     */
    QnCropImageFilter(const QRectF& rect, bool alignSize = true);

    virtual CLVideoDecoderOutputPtr updateImage(
        const CLVideoDecoderOutputPtr& frame,
        const QnAbstractCompressedMetadataPtr& metadata) override;

    virtual QSize updatedResolution(const QSize& srcSize) override;

private:
    bool m_alignSize;
    QRectF m_rectF;
    QRect m_rect;
    QSize m_size;
    CLVideoDecoderOutputPtr m_tmpRef;
};
