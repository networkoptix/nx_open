#include "image_button_widget.h"
#include <QPainter>
#include <QCursor>
#include <utils/common/warnings.h>

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

const QPixmap &QnImageButtonWidget::pixmap(PixmapRole role) const {
    if(role < 0 || role >= ROLE_COUNT) {
        qnWarning("Invalid pixmap role '%1'.", static_cast<int>(role));
        role = BACKGROUND_ROLE;
    }

    return m_pixmaps[role];
}

void QnImageButtonWidget::setPixmap(PixmapRole role, const QPixmap &pixmap) {
    if(role < 0 || role >= ROLE_COUNT) {
        qnWarning("Invalid pixmap role '%1'.", static_cast<int>(role));
        return;
    }

    m_pixmaps[role] = pixmap;
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
    if (m_checked && hasPixmap(CHECKED_ROLE)) {
        drawPixmap(painter, CHECKED_ROLE);
    } else if(m_underMouse && hasPixmap(HOVERED_ROLE)) {
        drawPixmap(painter, HOVERED_ROLE);
    } else {
        drawPixmap(painter, BACKGROUND_ROLE);
    }
}

bool QnImageButtonWidget::hasPixmap(PixmapRole role) {
    return !m_pixmaps[role].isNull();
}

void QnImageButtonWidget::drawPixmap(QPainter *painter, PixmapRole role) {
    painter->drawPixmap(rect(), m_pixmaps[role], m_pixmaps[role].rect());
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