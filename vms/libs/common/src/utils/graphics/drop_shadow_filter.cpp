#include "drop_shadow_filter.h"

#include <QtGui/QImage>
#include <QtGui/QPainter>

namespace nx::utils::graphics {

DropShadowFilter::DropShadowFilter(int dx, int dy, int blurWidth):
    m_blurFilter(blurWidth),
    m_dx(dx),
    m_dy(dy)
{
}

void DropShadowFilter::filterImage(QImage& image)
{
    QImage mask(image.size(), QImage::Format_ARGB32);
    mask.fill(qRgba(0, 0, 0, 0));

    QPainter(&mask).drawImage(m_dx, m_dy, image);
    m_blurFilter.filterImageAlpha(mask);
    QPainter(&mask).drawImage(QPoint(0, 0), image);

    image = mask;
}

} // namespace nx::utils::graphics