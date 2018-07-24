#pragma once

#include <QtGui/QPixmap>

class QSize;

namespace nx {
namespace core {

struct Watermark;

QPixmap getWatermarkImage(const Watermark& watermark, const QSize& size);

} // namespace core
} // namespace nx
