#include "image_button_widget.h"
#include <QPainter>
#include <QCursor>

QnImageButtonWidget::QnImageButtonWidget(QGraphicsItem *parent):
    base_type(parent),
    m_checkable(false), m_checked(false)
{
    setAcceptedMouseButtons(Qt::LeftButton);
    setClickableButtons(Qt::LeftButton);
    setAcceptsHoverEvents(true);

    /* When hovering over a button, a cursor should always change to arrow pointer. */
    setCursor(Qt::ArrowCursor);
}

void QnImageButtonWidget::setPixmap(const QPixmap &pixmap) {
    m_pixmap = pixmap;
    update();
}

void QnImageButtonWidget::setCheckedPixmap(const QPixmap &pixmap) {
    m_checkedPixmap = pixmap;
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
    if(m_pixmap.isNull()) {
        base_type::paint(painter, option, widget);
        return;
    }

    if (m_checked && !m_checkedPixmap.isNull())
        painter->drawPixmap(rect(), m_checkedPixmap, m_checkedPixmap.rect());
    else
        painter->drawPixmap(rect(), m_pixmap, m_pixmap.rect());
}

void QnImageButtonWidget::clickedNotify(QGraphicsSceneMouseEvent *) {
    Q_EMIT clicked();
    toggle();
}

void QnImageButtonWidget::hoverMoveEvent(QGraphicsSceneHoverEvent *event) {
    event->accept(); /* Buttons are opaque to hover events. */
}
