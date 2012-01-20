#include "image_button_widget.h"
#include <cassert>
#include <QPainter>
#include <QCursor>
#include <utils/common/warnings.h>
#include <utils/common/scoped_painter_rollback.h>
#include <ui/animation/accessor.h>
#include <ui/animation/variant_animator.h>
#include <ui/graphics/instruments/instrument_manager.h>

namespace {
    bool checkPixmapGroupRole(QnImageButtonWidget::StateFlags *flags) {
        bool result = true;

        if(*flags < 0 || *flags > QnImageButtonWidget::FLAGS_MAX) {
            qnWarning("Invalid pixmap flags '%1'.", static_cast<int>(*flags));
            *flags = 0;
            result = false;
        }

        return result;
    }

} // anonymous namespace

class QnImageButtonHoverProgressAccessor: public AbstractAccessor {
    virtual QVariant get(const QObject *object) const override {
        return static_cast<const QnImageButtonWidget *>(object)->m_hoverProgress;
    }

    virtual void set(QObject *object, const QVariant &value) const override {
        QnImageButtonWidget *widget = static_cast<QnImageButtonWidget *>(object);
        if(qFuzzyCompare(widget->m_hoverProgress, value.toReal()))
            return;

        widget->m_hoverProgress = value.toReal();
        widget->update();
    }
};

QnImageButtonWidget::QnImageButtonWidget(QGraphicsItem *parent):
    base_type(parent),
    m_checkable(false), 
    m_state(0),
    m_hoverProgress(0.0)
{
    setAcceptedMouseButtons(Qt::LeftButton);
    setClickableButtons(Qt::LeftButton);
    setAcceptsHoverEvents(true);

    m_animator = new VariantAnimator(this);
    m_animator->setTargetObject(this);
    m_animator->setAccessor(new QnImageButtonHoverProgressAccessor());

    /* When hovering over a button, a cursor should always change to arrow pointer. */
    setCursor(Qt::ArrowCursor);

    /* Init animator timer. */
    itemChange(ItemSceneHasChanged, QVariant::fromValue<QGraphicsScene *>(scene()));
}

const QPixmap &QnImageButtonWidget::pixmap(StateFlags flags) const {
    checkPixmapGroupRole(&flags);

    return m_pixmaps[flags];
}

void QnImageButtonWidget::setPixmap(StateFlags flags, const QPixmap &pixmap) {
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
        updateState(m_state & ~CHECKED);
    update();
}

void QnImageButtonWidget::setChecked(bool checked)
{
    if (!m_checkable || checked == isChecked())
        return;

    updateState(checked ? (m_state | CHECKED) : (m_state & ~CHECKED));
    update();
}

void QnImageButtonWidget::setEnabled(bool enabled)
{
    setDisabled(!enabled);
}

void QnImageButtonWidget::setDisabled(bool disabled)
{
    if(isDisabled() == disabled)
        return;

    updateState(disabled ? (m_state | DISABLED) : (m_state & ~DISABLED));
    update();
}

qreal QnImageButtonWidget::animationSpeed() const {
    return m_animator->speed();
}

void QnImageButtonWidget::setAnimationSpeed(qreal animationSpeed) {
    m_animator->setSpeed(animationSpeed);
}

void QnImageButtonWidget::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    const QPixmap &hoverPixmap = actualPixmap(m_state | HOVERED);
    const QPixmap &normalPixmap = actualPixmap(m_state & ~HOVERED);

    if(&hoverPixmap == &normalPixmap) {
        painter->drawPixmap(rect(), normalPixmap, normalPixmap.rect());
    } else {
        qreal o = painter->opacity();
        qreal k = m_hoverProgress;

        /* Calculate layer opacities so that total opacity is 'o'. */
        qreal o1 = (o - k * o) / (1 - k * o);
        qreal o2 = k * o;

        if(!qFuzzyIsNull(o1)) {
            painter->setOpacity(o1);
            painter->drawPixmap(rect(), normalPixmap, normalPixmap.rect());
        }
        if(!qFuzzyIsNull(o2)) {
            painter->setOpacity(o2);
            painter->drawPixmap(rect(), hoverPixmap,  hoverPixmap.rect());
        }

        painter->setOpacity(o);
    }
}

void QnImageButtonWidget::clickedNotify(QGraphicsSceneMouseEvent *) {
    if(isDisabled())
        return;

    toggle();
    Q_EMIT clicked(isChecked());
}

void QnImageButtonWidget::hoverEnterEvent(QGraphicsSceneHoverEvent *event) {
    event->accept(); /* Buttons are opaque to hover events. */

    updateState(m_state | HOVERED);
}

void QnImageButtonWidget::hoverMoveEvent(QGraphicsSceneHoverEvent *event) {
    event->accept(); /* Buttons are opaque to hover events. */

    updateState(m_state | HOVERED); /* In case we didn't receive the hover enter event. */
}

void QnImageButtonWidget::hoverLeaveEvent(QGraphicsSceneHoverEvent *event) {
    event->accept(); /* Buttons are opaque to hover events. */

    updateState(m_state & ~HOVERED);
}

QVariant QnImageButtonWidget::itemChange(GraphicsItemChange change, const QVariant &value) {
    switch(change) {
    case ItemSceneHasChanged:
        m_animator->setTimer(InstrumentManager::animationTimerOf(scene()));
        break;
    default:
        break;
    }

    return base_type::itemChange(change, value);
}

const QPixmap &QnImageButtonWidget::actualPixmap(StateFlags flags) {
    const QPixmap &standard = m_pixmaps[0];

    /* Some compilers don't allow expressions in case labels, so we have to
     * precalculate them. */
    enum {
        CHECKED_HOVERED_DISABLED = CHECKED | HOVERED | DISABLED,
        CHECKED_HOVERED = CHECKED | HOVERED,
        CHECKED_DISABLED = CHECKED | DISABLED,
        HOVERED_DISABLED = HOVERED | DISABLED,
    };

    switch(flags) {
#define TRY(FLAGS)                                                              \
        if(!m_pixmaps[(FLAGS)].isNull())                                        \
            return m_pixmaps[(FLAGS)];
    case CHECKED_HOVERED_DISABLED:
        TRY(CHECKED | HOVERED | DISABLED);
        TRY(CHECKED | DISABLED);
        TRY(CHECKED);
        return standard;
    case CHECKED_HOVERED:
        TRY(CHECKED | HOVERED);
        TRY(CHECKED);
        return standard;
    case CHECKED_DISABLED:
        TRY(CHECKED | DISABLED);
        TRY(CHECKED);
        return standard;
    case HOVERED_DISABLED:
        TRY(HOVERED | DISABLED);
        TRY(DISABLED);
        return standard;
    case CHECKED:
        TRY(CHECKED);
        return standard;
    case HOVERED:
        TRY(HOVERED);
        return standard;
    case DISABLED:
        TRY(DISABLED);
        return standard;
    case 0:
        return standard;
    default:
        return standard;
#undef TRY
    }
}

void QnImageButtonWidget::updateState(StateFlags state) {
    if(m_state == state)
        return;

    StateFlags oldState = m_state;
    m_state = state;

    if((oldState ^ m_state) & CHECKED)
        Q_EMIT toggled(isChecked());

    m_animator->animateTo(isHovered() ? 1.0 : 0.0);
}
