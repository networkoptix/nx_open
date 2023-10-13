// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>

#include <libavutil/pixfmt.h>

#include "abstract_image_filter.h"

class NX_VMS_COMMON_API QnScaleImageFilter: public QnAbstractImageFilter
{
public:
    QnScaleImageFilter(const QSize& size, AVPixelFormat format = AV_PIX_FMT_NONE);

    /** Returns the original frame if scaling has failed. */
    virtual CLVideoDecoderOutputPtr updateImage(const CLVideoDecoderOutputPtr& frame) override;

    virtual QSize updatedResolution(const QSize& srcSize) override;

    void setOutputImageSize( const QSize& size );

private:
    QSize m_size;
    AVPixelFormat m_format;
};
