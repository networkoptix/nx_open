#pragma once

#include <QtCore/QSize>
#include <QtGui/QPixmap>

#include <nx/core/watermark/watermark.h>

class QPixmap;
class QPainter;

namespace nx {
namespace client {
namespace desktop {

class WatermarkPainter
{
public:
    // Set bypassWatermarkCache to prevent watermark cache drop when using custom temporary watermark.
    WatermarkPainter(bool bypassWatermarkCache = false);

    void drawWatermark(QPainter* painter, const QRectF& rect);

    void setWatermark(nx::core::Watermark watermark);

private:
    void updateWatermarkImage(const QSize& size);

    nx::core::Watermark m_watermark;

    QPixmap m_pixmap;
    bool m_bypassCache;
};

} // namespace desktop
} // namespace client
} // namespace nx

