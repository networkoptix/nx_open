#include "tool_tip_slider.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QStyleOptionSlider>
#include <QtWidgets/QGraphicsSceneMouseEvent>

#include <utils/common/checked_cast.h>

#include <ui/animation/opacity_animator.h>
#include <ui/graphics/items/generic/styled_tooltip_widget.h>

#include "tool_tip_widget.h"

namespace {
    const int toolTipHideDelay = 2500;

} // anonymous namespace

/**
 * Note that this is a separate class so that user classes derived from
 * <tt>QnToolTipSlider</tt> can freely inherit <tt>AnimationTimerListener</tt>
 * without any conflicts.
 */
class QnToolTipSliderAnimationListener: public AnimationTimerListener {
public:
    QnToolTipSliderAnimationListener(QnToolTipSlider *slider): m_slider(slider) {
        startListening();
    }

    virtual void tick(int) override;

private:
    QnToolTipSlider *m_slider;
};

class QnToolTipSliderVisibilityAccessor: public AbstractAccessor {
public:
    virtual QVariant get(const QObject *object) const override {
        return checked_cast<const QnToolTipSlider *>(object)->m_tooltipWidgetVisibility;
    }

    virtual void set(QObject *object, const QVariant &value) const override {
        checked_cast<QnToolTipSlider *>(object)->m_tooltipWidgetVisibility = value.toReal();
    }
};


QnToolTipSlider::QnToolTipSlider(QGraphicsItem *parent):
    base_type(Qt::Horizontal, parent)
{
    init();
}

QnToolTipSlider::QnToolTipSlider(GraphicsSliderPrivate &dd, QGraphicsItem *parent):
    base_type(dd, parent)
{
    init();
}

void QnToolTipSlider::init() {
    setOrientation(Qt::Horizontal);

    m_tooltipWidgetVisibility = 0.0;
    m_autoHideToolTip = true;
    m_sliderUnderMouse = false;
    m_toolTipUnderMouse = false;
    m_pendingPositionUpdate = false;
    m_instantPositionUpdate = false;

    m_tooltipWidgetVisibilityAnimator = new VariantAnimator(this);
    m_tooltipWidgetVisibilityAnimator->setSpeed(2.0);
    m_tooltipWidgetVisibilityAnimator->setAccessor(new QnToolTipSliderVisibilityAccessor());
    m_tooltipWidgetVisibilityAnimator->setTargetObject(this);
    registerAnimation(m_tooltipWidgetVisibilityAnimator);

    m_animationListener.reset(new QnToolTipSliderAnimationListener(this));
    registerAnimation(m_animationListener.data());

    setToolTipItem(new QnStyledTooltipWidget(this));
    setAcceptHoverEvents(true);

    setFlag(ItemSendsScenePositionChanges, true);
}

QnToolTipSlider::~QnToolTipSlider() {
    m_hideTimer.stop();
}

QnToolTipWidget *QnToolTipSlider::toolTipItem() const {
    return m_tooltipWidget.data();
}

void QnToolTipSlider::setToolTipItem(QnToolTipWidget *newToolTipItem) {
    qreal opacity = 0.0;
    if(toolTipItem()) {
        opacity = toolTipItem()->opacity();
        delete toolTipItem();
    }

    m_tooltipWidget = newToolTipItem;

    if(toolTipItem()) {
        toolTipItem()->setParent(this); /* Claim ownership, but not in graphics item sense. */
        toolTipItem()->setFocusProxy(this);
        toolTipItem()->setOpacity(opacity);
        toolTipItem()->setAcceptHoverEvents(true);
        toolTipItem()->installEventFilter(this);
        toolTipItem()->setFlag(QGraphicsItem::ItemIgnoresParentOpacity, true);
        connect(toolTipItem(), SIGNAL(tailPosChanged()), this, SLOT(updateToolTipPosition()));

        updateToolTipText();
        updateToolTipOpacity();
        updateToolTipPosition();
        updateToolTipVisibility();
    }
}

bool QnToolTipSlider::isToolTipAutoHidden() const {
    return m_autoHideToolTip;
}

void QnToolTipSlider::setAutoHideToolTip(bool autoHideToolTip) {
    m_autoHideToolTip = autoHideToolTip;

    updateToolTipVisibility();
}

void QnToolTipSlider::hideToolTip() {
    m_tooltipWidgetVisibilityAnimator->animateTo(0.0);
}

void QnToolTipSlider::showToolTip() {
    m_tooltipWidgetVisibilityAnimator->animateTo(1.0);
}

void QnToolTipSlider::updateToolTipVisibility() {
    if(!toolTipItem())
        return;

    if(m_autoHideToolTip) {
        if(m_sliderUnderMouse || m_toolTipUnderMouse || m_hideTimer.isActive()) {
            showToolTip();
        } else {
            hideToolTip();
        }
    } else {
        showToolTip();
    }
}

void QnToolTipSlider::updateToolTipOpacity() {
    if(!toolTipItem())
        return;

    toolTipItem()->setOpacity(effectiveOpacity() * m_tooltipWidgetVisibility);
}

void QnToolTipSlider::updateToolTipText() {
    if(!toolTipItem())
        return;

    toolTipItem()->setText(toolTip());
}

void QnToolTipSlider::updateToolTipPosition() {
    if(!m_instantPositionUpdate) {
        m_pendingPositionUpdate = true;
        return;
    }

    m_pendingPositionUpdate = false;

    if(!toolTipItem())
        return;

    QStyleOptionSlider opt;
    initStyleOption(&opt);
    QRect handleRect = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, NULL);

    qreal x = positionFromValue(sliderPosition()).x() + handleRect.width() / 2.0;
    qreal y = handleRect.top();

    toolTipItem()->pointTo(toolTipItem()->mapToParent(toolTipItem()->mapFromItem(this, x, y)));
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnToolTipSliderAnimationListener::tick(int) {
    /* Unfortunately, there is no notification for a change in effective opacity,
     * so we have to track it in animation handler (which gets invoked before the
     * paint event). */
    m_slider->updateToolTipOpacity();

    /* All position updates in [tick, paint] time period are instant. */
    m_slider->m_instantPositionUpdate = true;

    if(m_slider->m_pendingPositionUpdate)
        m_slider->updateToolTipPosition();
}

void QnToolTipSlider::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    m_instantPositionUpdate = false;

    base_type::paint(painter, option, widget);
}

QString QnToolTipSlider::toolTipAt(const QPointF &pos) const {
    Q_UNUSED(pos)
    /* Default tooltip is meaningless for this slider,
     * so we don't want it to be shown. */
    return QString();
}

bool QnToolTipSlider::showOwnTooltip(const QPointF &pos) {
    Q_UNUSED(pos)
    /* Default tooltip is meaningless for this slider,
     * so we don't want it to be shown.
     * Displaying is also controlled by the slider itself.
     */
    return true;
}

bool QnToolTipSlider::eventFilter(QObject *target, QEvent *event) {
    if(target == toolTipItem()) {
        QGraphicsSceneMouseEvent *e = static_cast<QGraphicsSceneMouseEvent *>(event);

        switch(event->type()) {
        case QEvent::GraphicsSceneMousePress:
            if(e->button() == Qt::LeftButton) {
                setSliderDown(true);
                m_dragOffset = positionFromValue(sliderPosition()) - toolTipItem()->mapToItem(this, e->pos());
                e->accept();
                return true;
            }
            break;
        case QEvent::GraphicsSceneMouseMove:
            if(isSliderDown()) {
                qint64 pos = valueFromPosition(toolTipItem()->mapToItem(this, e->pos()) + m_dragOffset);
                setSliderPosition(pos);

                e->accept();
                return true;
            }
            break;
        case QEvent::GraphicsSceneMouseRelease:
            if(e->button() == Qt::LeftButton) {
                setSliderDown(false);

                e->accept();
                return true;
            }
            break;
        case QEvent::GraphicsSceneHoverEnter:
            m_toolTipUnderMouse = true;
            updateToolTipVisibility();
            break;
        case QEvent::GraphicsSceneHoverLeave:
            m_toolTipUnderMouse = false;
            m_hideTimer.start(toolTipHideDelay, this);
            updateToolTipVisibility();
            break;
        default:
            break;
        }
        return false;
    } else {
        return base_type::eventFilter(target, event);
    }
}

void QnToolTipSlider::sliderChange(SliderChange change) {
    base_type::sliderChange(change);

    if(toolTipItem()) {
        if(change == SliderValueChange) {
            if(m_autoHideToolTip)
                m_hideTimer.start(toolTipHideDelay, this);
            updateToolTipVisibility();
            updateToolTipPosition();
        } else if(change == SliderMappingChange) {
            updateToolTipPosition();
        }
    }
}

QVariant QnToolTipSlider::itemChange(GraphicsItemChange change, const QVariant &value) {
    QVariant result = base_type::itemChange(change, value);

    switch(change) {
    case ItemToolTipHasChanged:
        updateToolTipText();
        break;
    case ItemOpacityHasChanged:
        updateToolTipOpacity();
        break;
    case ItemScenePositionHasChanged:
        updateToolTipPosition();
        break;
    default:
        break;
    }

    return result;
}

void QnToolTipSlider::timerEvent(QTimerEvent *event) {
    base_type::timerEvent(event);

    if(event->timerId() == m_hideTimer.timerId()) {
        m_hideTimer.stop();

        updateToolTipVisibility();
    }
}

void QnToolTipSlider::hoverEnterEvent(QGraphicsSceneHoverEvent *event) {
    base_type::hoverEnterEvent(event);
    m_sliderUnderMouse = true;

    updateToolTipVisibility();
}

void QnToolTipSlider::hoverLeaveEvent(QGraphicsSceneHoverEvent *event) {
    base_type::hoverLeaveEvent(event);
    m_sliderUnderMouse = false;
    m_hideTimer.start(toolTipHideDelay, this);

    updateToolTipVisibility();
}

void QnToolTipSlider::resizeEvent(QGraphicsSceneResizeEvent *event) {
    base_type::resizeEvent(event);

    updateToolTipPosition();
}

