#include "image_button_widget.h"
#include <cassert>
#include <QPainter>
#include <QCursor>
#include <utils/common/warnings.h>

namespace {
    bool checkPixmapGroupRole(QnImageButtonWidget::PixmapFlags *flags) {
        bool result = true;

        if(*flags < 0 || *flags > QnImageButtonWidget::FLAGS_MAX) {
            qnWarning("Invalid pixmap flags '%1'.", static_cast<int>(*flags));
            *flags = 0;
            result = false;
        }

        return result;
    }

} // anonymous namespace

QnImageButtonWidget::QnImageButtonWidget(QGraphicsItem *parent):
    base_type(parent),
    m_checkable(false), 
    m_checked(false),
    m_underMouse(false)
{
    setAcceptedMouseButtons(Qt::LeftButton);
    setClickableButtons(Qt::LeftButton);
    setAcceptsHoverEvents(true);

    /* When hovering over a button, a cursor should always change to arrow pointer. */
    setCursor(Qt::ArrowCursor);
}

const QPixmap &QnImageButtonWidget::pixmap(PixmapFlags flags) const {
    checkPixmapGroupRole(&flags);

    return m_pixmaps[flags];
}

void QnImageButtonWidget::setPixmap(PixmapFlags flags, const QPixmap &pixmap) {
    if(!checkPixmapGroupRole(&flags))
        return;

    m_pixmaps[flags] = pixmap;
    update();
}

void QnImageButtonWidget::setCheckable(bool checkable)
{
    if (m_checkable == checkable)
        return;

    m_checkable = checkable;
    if (!m_checkable)
        m_checked = false;
    update();
}

void QnImageButtonWidget::setChecked(bool checked)
{
    if (!m_checkable || m_checked == checked)
        return;

    m_checked = checked;
    update();
    Q_EMIT toggled(m_checked);
}

void QnImageButtonWidget::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
#define STATE(IS_CHECKED, IS_UNDER_MOUSE) (((int)IS_CHECKED << 1) | (int)IS_UNDER_MOUSE)
    switch(STATE(m_checked, m_underMouse)) {
    case STATE(true, true):
        if(drawPixmap(painter, CHECKED | HOVERED))
            break;
    case STATE(true, false):
        if(drawPixmap(painter, CHECKED))
            break;
    case STATE(false, true):
        if(drawPixmap(painter, HOVERED))
            break;
    case STATE(false, false):
        if(drawPixmap(painter, 0))
            break;
    default:
        break;
    }
#undef STATE
}

bool QnImageButtonWidget::drawPixmap(QPainter *painter, PixmapFlags flags) {
    if(m_pixmaps[flags].isNull()) {
        return false;
    } else {
        painter->drawPixmap(rect(), m_pixmaps[flags], m_pixmaps[flags].rect());
        return true;
    }
}

void QnImageButtonWidget::clickedNotify(QGraphicsSceneMouseEvent *) {
    Q_EMIT clicked();
    toggle();
}

void QnImageButtonWidget::hoverEnterEvent(QGraphicsSceneHoverEvent *event) {
    event->accept(); /* Buttons are opaque to hover events. */

    setUnderMouse(true);
}

void QnImageButtonWidget::hoverMoveEvent(QGraphicsSceneHoverEvent *event) {
    event->accept(); /* Buttons are opaque to hover events. */

    setUnderMouse(true); /* In case we didn't receive the hover enter event. */
}

void QnImageButtonWidget::hoverLeaveEvent(QGraphicsSceneHoverEvent *event) {
    event->accept(); /* Buttons are opaque to hover events. */

    setUnderMouse(false);
}

void QnImageButtonWidget::setUnderMouse(bool underMouse) {
    if(m_underMouse == underMouse)
        return;

    m_underMouse = underMouse;
    if(m_underMouse) {
        Q_EMIT hoverEntered();
    } else {
        Q_EMIT hoverLeft();
    }
}