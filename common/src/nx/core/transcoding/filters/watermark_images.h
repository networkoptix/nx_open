#pragma once

#include <QtGui/QPixmap>

class QSize;
class QString;
struct QnWatermarkSettings;

namespace nx {
namespace core {
namespace transcoding {

QPixmap getWatermarkImage(const QnWatermarkSettings& settings, const QString& text, const QSize& size);

} // namespace transcoding
} // namespace core
} // namespace nx
