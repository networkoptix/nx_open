// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "scale_image_filter.h"

#include <nx/utils/log/assert.h>
#include <utils/media/frame_info.h>

QnScaleImageFilter::QnScaleImageFilter(
    const QSize& size,
    AVPixelFormat format)
    :
    m_size(size),
    m_format(format)
{
}

CLVideoDecoderOutputPtr QnScaleImageFilter::updateImage(const CLVideoDecoderOutputPtr& frame)
{
    auto result = frame->scaled(m_size, m_format);

    // `scaled()` can return an empty pointer. Better process as is than crash.
    return NX_ASSERT(result, "Error while scaling frame to %1 (%2)", m_size, m_format)
        ? result
        : frame;
}

QSize QnScaleImageFilter::updatedResolution(const QSize& /*srcSize*/)
{
    return m_size;
}

void QnScaleImageFilter::setOutputImageSize( const QSize& size )
{
    m_size = size;
}
