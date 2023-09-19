// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSize>
#include <QtGui/QPixmap>

#include <nx/core/watermark/watermark.h>

class QPixmap;
class QPainter;

namespace nx::vms::client::desktop {

class WatermarkPainter
{
public:
    WatermarkPainter();

    void drawWatermark(QPainter* painter, const QRectF& rect);

    void setWatermark(nx::core::Watermark watermark);
    const nx::core::Watermark& watermark() const;

private:
    void updateWatermarkImage(const QSize& size);

    nx::core::Watermark m_watermark;

    QSize m_size;
    QPixmap m_pixmap;
};

} // namespace nx::vms::client::desktop
