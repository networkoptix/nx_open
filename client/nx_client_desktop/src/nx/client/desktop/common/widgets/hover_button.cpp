#include "hover_button.h"

#include <QtWidgets/QVBoxLayout>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>

#include <ui/style/globals.h>
#include <ui/style/skin.h>
#include <ui/style/nx_style.h>
#include <ui/style/helper.h>

namespace nx {
namespace client {
namespace desktop {


HoverButton::HoverButton(const QString& normal, const QString& highlighted, QWidget* parent) :
    HoverButton(normal, highlighted, QString(), parent)
{
}

HoverButton::HoverButton(const QString& normal, const QString& highlighted, const QString& pressed,
    QWidget* parent) : QAbstractButton(parent)
{
    m_normal = qnSkin->pixmap(normal, true);
    m_highlighted = qnSkin->pixmap(highlighted, true);
    m_hasPressedState = !pressed.isEmpty();
    if (m_hasPressedState)
        m_pressed = qnSkin->pixmap(pressed, true);

    installEventFilter(this);

    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    // For hovering stuff
    setMouseTracking(true);
}

QSize HoverButton::sizeHint() const
{
    return m_normal.size();
}

void HoverButton::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);

    QPixmap pixmap = m_normal;
    if (m_hasPressedState && isDown())
        pixmap = m_pressed;
    if (m_isHovered && !isDown())
        pixmap = m_highlighted;

    if (!pixmap.isNull())
    {
        auto icon = pixmap.rect();
        QPointF centeredCorner = rect().center() - icon.center();
        painter.drawPixmap(centeredCorner * 0.5, pixmap);
    }
}

void HoverButton::mouseMoveEvent(QMouseEvent* event)
{
    bool hovered = rect().contains(event->pos());
    if (hovered != m_isHovered)
    {
        m_isHovered = hovered;
        update();
    }
}

void HoverButton::leaveEvent(QEvent* event)
{
    if (m_isHovered)
    {
        m_isHovered = false;
        update();
    }
}

} // namespace desktop
} // namespace client
} // namespace nx
