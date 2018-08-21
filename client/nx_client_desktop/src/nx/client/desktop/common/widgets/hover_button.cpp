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

HoverButton::HoverButton(const QString& normal, const QString& highligthed, QWidget* parent):
    QAbstractButton(parent)
{
    m_normal = qnSkin->pixmap(normal, true);
    m_highlighted = qnSkin->pixmap(highligthed, true);
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
    bool highlighted = false;
    if (!isDown())
        highlighted = m_isHovered;

    QPixmap& pixmap = highlighted ? m_highlighted : m_normal;
    if (!pixmap.isNull())
    {
        auto icon = pixmap.rect();
        QPointF centeredCorner = rect().center() - icon.center();
        painter.drawPixmap(centeredCorner * 0.5, pixmap);
    }
}

void HoverButton::mouseMoveEvent(QMouseEvent* event)
{
    base_type::mouseMoveEvent(event);

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
