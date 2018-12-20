#include "hover_button.h"

#include <QtGui/QPainter>
#include <QtWidgets/QStyle>

#include <ui/style/skin.h>

namespace nx::vms::client::desktop {

HoverButton::HoverButton(const QString& normalPixmap, const QString& hoveredPixmap, QWidget* parent):
    HoverButton(normalPixmap, hoveredPixmap, QString(), parent)
{
}

HoverButton::HoverButton(
    const QString& normalPixmap,
    const QString& hoveredPixmap,
    const QString& pressedPixmap,
    QWidget* parent)
    :
    base_type(parent),
    m_normal(normalPixmap.isEmpty() ? QPixmap() : qnSkin->pixmap(normalPixmap)),
    m_hovered(hoveredPixmap.isEmpty() ? QPixmap() : qnSkin->pixmap(hoveredPixmap)),
    m_pressed(pressedPixmap.isEmpty() ? QPixmap() : qnSkin->pixmap(pressedPixmap))
{
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setMouseTracking(true);
    setAttribute(Qt::WA_Hover);
}

QSize HoverButton::sizeHint() const
{
    return m_normal.size() / m_normal.devicePixelRatio();
}

void HoverButton::paintEvent(QPaintEvent* /*event*/)
{
    QPixmap pixmap;
    if (isDown())
        pixmap = m_pressed.isNull() ? m_normal : m_pressed;
    else if (underMouse())
        pixmap = m_hovered.isNull() ? m_normal : m_hovered;
    else
        pixmap = m_normal;

    if (pixmap.isNull())
        return;

    const auto pixmapRect = QStyle::alignedRect(
        Qt::LeftToRight,
        Qt::AlignCenter,
        pixmap.size() / pixmap.devicePixelRatio(),
        rect());

    QPainter painter(this);
    painter.drawPixmap(pixmapRect, pixmap);
}

} // namespace nx::vms::client::desktop
