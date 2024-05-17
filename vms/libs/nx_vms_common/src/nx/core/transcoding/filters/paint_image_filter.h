// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QPoint>

#include <transcoding/filters/abstract_image_filter.h>

namespace nx {
namespace core {
namespace transcoding {

namespace detail {

class ImageToFramePainter;

} // namespace detail

class PaintImageFilter: public QnAbstractImageFilter
{
public:
    PaintImageFilter();
    virtual ~PaintImageFilter();

    virtual CLVideoDecoderOutputPtr updateImage(
        const CLVideoDecoderOutputPtr& frame,
        const QnAbstractCompressedMetadataPtr& metadata) override;

    virtual QSize updatedResolution(const QSize& sourceSize) override;

    void setImage(
        const QImage& image,
        const QPoint& offset = QPoint(0, 0),
        Qt::Alignment alignment = Qt::AlignLeft | Qt::AlignTop);

private:
    const QScopedPointer<detail::ImageToFramePainter> m_painter;
};

} // namespace transcoding
} // namespace core
} // namespace nx
