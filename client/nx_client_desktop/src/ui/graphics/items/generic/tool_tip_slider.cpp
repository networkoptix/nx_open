#include "tool_tip_slider.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QStyleOptionSlider>
#include <QtWidgets/QGraphicsSceneMouseEvent>

#include <utils/common/checked_cast.h>

#include <nx/client/desktop/ui/workbench/workbench_animations.h>

#include <ui/animation/opacity_animator.h>
#include <ui/graphics/items/generic/styled_tooltip_widget.h>
#include <ui/graphics/items/generic/slider_tooltip_widget.h>

#include <nx/utils/math/fuzzy.h>

namespace {

const int kDefaultToolTipHideDelayMs = 2500;

} // namespace

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

    virtual void set(QObject *object, const QVariant &value) const override
    {
        checked_cast<QnToolTipSlider *>(object)->m_tooltipWidgetVisibility = value.toReal();
    }
};

QnToolTipSlider::QnToolTipSlider(QGraphicsItem* parent):
    base_type(Qt::Horizontal, parent),
    m_tooltipWidgetVisibilityAnimator(new VariantAnimator(this)),
    m_tooltipWidgetVisibility(0),
    m_autoHideToolTip(true),
    m_sliderUnderMouse(false),
    m_toolTipUnderMouse(false),
    m_pendingPositionUpdate(false),
    m_instantPositionUpdate(false),
    m_tooltipMargin(0),
    m_toolTipEnabled(true),
    m_toolTipHideDelayMs(kDefaultToolTipHideDelayMs)
{
    setOrientation(Qt::Horizontal);

    m_tooltipWidgetVisibilityAnimator->setAccessor(new QnToolTipSliderVisibilityAccessor());
    m_tooltipWidgetVisibilityAnimator->setTargetObject(this);
    registerAnimation(m_tooltipWidgetVisibilityAnimator);

    m_animationListener.reset(new QnToolTipSliderAnimationListener(this));
    registerAnimation(m_animationListener.data() );

    setToolTipItem(new QnSliderTooltipWidget(this));
    setAcceptHoverEvents(true);

    setFlag(ItemSendsScenePositionChanges, true);
}

QnToolTipSlider::~QnToolTipSlider() {
    m_hideTimer.stop();
}

qreal QnToolTipSlider::tooltipMargin() const
{
    return m_tooltipMargin;
}

void QnToolTipSlider::setTooltipMargin(qreal margin)
{
    if (qFuzzyEquals(margin, m_tooltipMargin))
        return;

    m_tooltipMargin = margin;
    updateToolTipPosition();
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

void QnToolTipSlider::hideToolTip(bool animated)
{
    using namespace nx::client::desktop::ui::workbench;
    qnWorkbenchAnimations->setupAnimator(m_tooltipWidgetVisibilityAnimator,
        Animations::Id::TimelineTooltipHide);

    //TODO: #GDM we certainly need to find place for these constants
    const qreal kTransparent = 0.0;
    if (animated)
    {
        m_tooltipWidgetVisibilityAnimator->animateTo(kTransparent);
    }
    else
    {
        m_tooltipWidgetVisibilityAnimator->stop();
        m_tooltipWidgetVisibility = kTransparent;
    }
}

void QnToolTipSlider::setToolTipEnabled(bool enabled)
{
    if (enabled == m_toolTipEnabled)
        return;

    m_toolTipEnabled = enabled;

    if (!enabled)
        hideToolTip(false);
}

void QnToolTipSlider::showToolTip(bool animated)
{
    using namespace nx::client::desktop::ui::workbench;
    qnWorkbenchAnimations->setupAnimator(m_tooltipWidgetVisibilityAnimator,
        Animations::Id::TimelineTooltipShow);

    const qreal opacity = (m_toolTipEnabled ? 1.0 : 0.0);
    if (animated)
    {
        m_tooltipWidgetVisibilityAnimator->animateTo(opacity);
    }
    else
    {
        m_tooltipWidgetVisibilityAnimator->stop();
        m_tooltipWidgetVisibility = opacity;
    }
}

void QnToolTipSlider::updateToolTipVisibility()
{
    /* Check if tooltip visibility is controlled manually */
    if (!m_autoHideToolTip)
        return;

    if(!toolTipItem())
        return;

    if (m_sliderUnderMouse || m_toolTipUnderMouse || m_hideTimer.isActive())
        showToolTip();
    else
        hideToolTip();
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
    QRect handleRect = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, nullptr);

    qreal x = positionFromValue(sliderPosition()).x() + handleRect.width() / 2.0;
    qreal y = -m_tooltipMargin;

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
            m_hideTimer.start(m_toolTipHideDelayMs, this);
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
                m_hideTimer.start(m_toolTipHideDelayMs, this);
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
    m_hideTimer.start(m_toolTipHideDelayMs, this);

    updateToolTipVisibility();
}

void QnToolTipSlider::resizeEvent(QGraphicsSceneResizeEvent *event) {
    base_type::resizeEvent(event);

    updateToolTipPosition();
}

int QnToolTipSlider::toolTipHideDelayMs() const
{
    return m_toolTipHideDelayMs;
}

void QnToolTipSlider::setToolTipHideDelayMs(int delayMs)
{
    m_toolTipHideDelayMs = delayMs;
}
