#include "image_button_widget.h"
#include <cassert>
#include <QPainter>
#include <QIcon>
#include <QAction>
#include <utils/common/warnings.h>
#include <utils/common/scoped_painter_rollback.h>
#include <ui/animation/accessor.h>
#include <ui/animation/variant_animator.h>
#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/common/scene_utility.h>

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

    QPixmap bestPixmap(const QIcon &icon, QIcon::Mode mode, QIcon::State state) {
        QList<QSize> sizes = icon.availableSizes(mode, state);
        if(sizes.empty())
            return QPixmap();

        QSize bestSize(0, 0);
        foreach(const QSize &size, sizes)
            if(size.width() > bestSize.width())
                bestSize = size;

        return icon.pixmap(bestSize, mode, state);
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
    m_pixmapCacheValid(false),
    m_checkable(false), 
    m_cached(false),
    m_state(0),
    m_hoverProgress(0.0),
    m_action(NULL)
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

const QPixmap &QnImageButtonWidget::cachedPixmap(StateFlags flags) {
    if(!m_cached) {
        return m_pixmaps[flags];
    } else {
        ensurePixmapCache();
        
        return m_pixmapCache[flags];
    }
}

void QnImageButtonWidget::setPixmap(StateFlags flags, const QPixmap &pixmap) {
    if(!checkPixmapGroupRole(&flags))
        return;

    m_pixmaps[flags] = pixmap;

    invalidatePixmapCache();
    update();
}

void QnImageButtonWidget::setIcon(const QIcon &icon) {
    for(int i = 0; i <= FLAGS_MAX; i++)
        m_pixmaps[i] = QPixmap();

    setPixmap(0,                    bestPixmap(icon, QIcon::Normal, QIcon::Off));
    setPixmap(HOVERED,              bestPixmap(icon, QIcon::Active, QIcon::Off));
    setPixmap(DISABLED,             bestPixmap(icon, QIcon::Disabled, QIcon::Off));
    setPixmap(PRESSED,              bestPixmap(icon, QIcon::Selected, QIcon::Off));

    setPixmap(CHECKED,              bestPixmap(icon, QIcon::Normal, QIcon::On));
    setPixmap(CHECKED | HOVERED,    bestPixmap(icon, QIcon::Active, QIcon::On));
    setPixmap(CHECKED | DISABLED,   bestPixmap(icon, QIcon::Disabled, QIcon::On));
    setPixmap(CHECKED | PRESSED,    bestPixmap(icon, QIcon::Selected, QIcon::On));
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

void QnImageButtonWidget::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
    StateFlags hoverState = displayState(m_state | HOVERED);
    StateFlags normalState = displayState(m_state & ~HOVERED);

    qreal o = painter->opacity();
    qreal k = m_hoverProgress;

    /* Calculate layer opacities so that total opacity is 'o'. */
    qreal o1 = (o - k * o) / (1 - k * o);
    qreal o2 = k * o;

    if(!qFuzzyIsNull(o1)) {
        painter->setOpacity(o1);
        paintPixmap(painter, normalState, m_state & ~HOVERED);
    }
    if(!qFuzzyIsNull(o2)) {
        painter->setOpacity(o2);
        paintPixmap(painter, hoverState, m_state | HOVERED);
    }

    painter->setOpacity(o);
}

void QnImageButtonWidget::paintPixmap(QPainter *painter, StateFlags displayState, StateFlags actualState) {
    Q_UNUSED(actualState);

    const QPixmap pixmap = cachedPixmap(displayState);
    painter->drawPixmap(rect(), pixmap, pixmap.rect());
}

void QnImageButtonWidget::clickedNotify(QGraphicsSceneMouseEvent *) {
    if(isDisabled())
        return;

    toggle();
    Q_EMIT clicked(isChecked());

    if(m_action != NULL)
        m_action->trigger();
}

void QnImageButtonWidget::pressedNotify(QGraphicsSceneMouseEvent *event) {
    updateState(m_state | PRESSED);
    updateState(m_state & ~HOVERED);
}

void QnImageButtonWidget::releasedNotify(QGraphicsSceneMouseEvent *event) {
    updateState(m_state & ~PRESSED);
}

void QnImageButtonWidget::hoverEnterEvent(QGraphicsSceneHoverEvent *event) {
    event->accept(); /* Buttons are opaque to hover events. */

    updateState(m_state | HOVERED);

    base_type::hoverEnterEvent(event);
}

void QnImageButtonWidget::hoverMoveEvent(QGraphicsSceneHoverEvent *event) {
    event->accept(); /* Buttons are opaque to hover events. */

    updateState(m_state | HOVERED); /* In case we didn't receive the hover enter event. */

    base_type::hoverMoveEvent(event);
}

void QnImageButtonWidget::hoverLeaveEvent(QGraphicsSceneHoverEvent *event) {
    event->accept(); /* Buttons are opaque to hover events. */

    updateState(m_state & ~HOVERED);

    base_type::hoverLeaveEvent(event);
}

void QnImageButtonWidget::resizeEvent(QGraphicsSceneResizeEvent *event) {
    invalidatePixmapCache();

    base_type::resizeEvent(event);
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

QnImageButtonWidget::StateFlags QnImageButtonWidget::displayState(StateFlags flags) {
    /* Some compilers don't allow expressions in case labels, so we have to
     * precalculate them. */
    enum {
        CHECKED_HOVERED_DISABLED_PRESSED = CHECKED | HOVERED | DISABLED | PRESSED,
        CHECKED_HOVERED_DISABLED = CHECKED | HOVERED | DISABLED,
        CHECKED_HOVERED = CHECKED | HOVERED,
        CHECKED_DISABLED = CHECKED | DISABLED,
        HOVERED_DISABLED = HOVERED | DISABLED,
        CHECKED_HOVERED_PRESSED = CHECKED | HOVERED | PRESSED,
        CHECKED_DISABLED_PRESSED = CHECKED | DISABLED | PRESSED,
        HOVERED_DISABLED_PRESSED = HOVERED | DISABLED | PRESSED,
        CHECKED_PRESSED = CHECKED | PRESSED,
        HOVERED_PRESSED = HOVERED | PRESSED,
        DISABLED_PRESSED = DISABLED | PRESSED
    };

    switch(flags) {
#define TRY(FLAGS)                                                              \
        if(!m_pixmaps[(FLAGS)].isNull())                                        \
            return (FLAGS);
    case CHECKED_HOVERED_DISABLED_PRESSED:
        TRY(CHECKED | HOVERED | DISABLED | PRESSED);
        /* Fall through. */
    case CHECKED_HOVERED_DISABLED:
        TRY(CHECKED | HOVERED | DISABLED);
        TRY(CHECKED | DISABLED);
        TRY(CHECKED);
        return 0;
    case CHECKED_HOVERED_PRESSED:
        TRY(CHECKED | HOVERED | PRESSED);
        /* Fall through. */
    case CHECKED_HOVERED:
        TRY(CHECKED | HOVERED);
        TRY(CHECKED);
        return 0;
    case CHECKED_DISABLED_PRESSED:
        TRY(CHECKED | DISABLED | PRESSED);
        /* Fall through. */
    case CHECKED_DISABLED:
        TRY(CHECKED | DISABLED);
        TRY(CHECKED);
        return 0;
    case HOVERED_DISABLED_PRESSED:
        TRY(HOVERED | DISABLED | PRESSED);
        /* Fall through. */
    case HOVERED_DISABLED:
        TRY(HOVERED | DISABLED);
        TRY(DISABLED);
        return 0;
    case CHECKED_PRESSED:
        TRY(CHECKED | PRESSED);
        /* Fall through. */
    case CHECKED:
        TRY(CHECKED);
        return 0;
    case HOVERED_PRESSED:
        TRY(HOVERED | PRESSED);
        /* Fall through. */
    case HOVERED:
        TRY(HOVERED);
        return 0;
    case DISABLED_PRESSED:
        TRY(DISABLED | PRESSED);
        /* Fall through. */
    case DISABLED:
        TRY(DISABLED);
        return 0;
    case PRESSED:
        TRY(PRESSED);
        return 0;
    case 0:
        return 0;
    default:
        return 0;
#undef TRY
    }
}

void QnImageButtonWidget::updateState(StateFlags state) {
    if(m_state == state)
        return;

    StateFlags oldState = m_state;
    m_state = state;

    if((oldState ^ m_state) & CHECKED) {
        Q_EMIT toggled(isChecked());

        if(m_action != NULL)
            m_action->toggle();
    }

    if(m_action != NULL && !(oldState & HOVERED) && (m_state & HOVERED))
        m_action->hover();

    m_animator->animateTo(isHovered() ? 1.0 : 0.0);
}

void QnImageButtonWidget::setDefaultAction(QAction *action) {
    m_action = action;
    if (!action)
        return;

    if (!this->actions().contains(action))
        addAction(action);

    setIcon(action->icon());
    setToolTip(action->toolTip());
    
    setCheckable(action->isCheckable());
    setChecked(action->isChecked());
    setEnabled(action->isEnabled());
}

bool QnImageButtonWidget::event(QEvent *event) {
    QActionEvent *actionEvent = static_cast<QActionEvent *>(event);

    switch (event->type()) {
    case QEvent::ActionChanged:
        if (actionEvent->action() == m_action)
            setDefaultAction(actionEvent->action()); /** Update button state. */
        break;
    case QEvent::ActionAdded:
        break;
    case QEvent::ActionRemoved:
        if (actionEvent->action() == m_action)
            m_action = NULL;
        break;
    default:
        break;
    }

    return base_type::event(event);
}

bool QnImageButtonWidget::isCached() const {
    return m_cached;
}

void QnImageButtonWidget::setCached(bool cached) {
    if(m_cached == cached)
        return;

    m_cached = cached;
}

void QnImageButtonWidget::ensurePixmapCache() {
    if(m_pixmapCacheValid)
        return;

    for(int i = 0; i < FLAGS_MAX; i++) {
        if(m_pixmaps[i].isNull()) {
            m_pixmapCache[i] = m_pixmaps[i];
        } else {
            m_pixmapCache[i] = QPixmap::fromImage(m_pixmaps[i].toImage().scaled(size().toSize(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
        }
    }

    m_pixmapCacheValid = true;
}

void QnImageButtonWidget::invalidatePixmapCache() {
    m_pixmapCacheValid = false;
}


// -------------------------------------------------------------------------- //
// QnZoomingImageButtonWidget
// -------------------------------------------------------------------------- //
QnZoomingImageButtonWidget::QnZoomingImageButtonWidget(QGraphicsItem *parent):
    QnImageButtonWidget(parent)
{}

void QnZoomingImageButtonWidget::paintPixmap(QPainter *painter, StateFlags displayState, StateFlags actualState) {
    QRectF rect;
    if((actualState & HOVERED) && !(actualState & PRESSED)) {
        rect = this->rect();
    } else {
        rect = this->rect();

        QPointF d = SceneUtility::toPoint(rect.size()) * 0.075;
        rect = QRectF(
            rect.topLeft() + d,
            rect.bottomRight() - d
        );
    }

    const QPixmap pixmap = cachedPixmap(displayState);
    painter->drawPixmap(rect, pixmap, pixmap.rect());
}
