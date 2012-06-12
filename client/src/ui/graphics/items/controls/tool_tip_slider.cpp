#include "tool_tip_slider.h"

#include <QtGui/QStyleOptionSlider>
#include <QtGui/QGraphicsSceneMouseEvent>

#include <ui/animation/opacity_animator.h>

#include "tool_tip_item.h"

namespace {
    class QnSliderToolTipItem: public QnToolTipItem {
    public:
        QnSliderToolTipItem(QGraphicsItem *parent = 0) : QnToolTipItem(parent)
        {
            setFont(QFont()); /* Default application font. */
            setTextPen(QColor(63, 159, 216));
            setBrush(QColor(0, 0, 0, 255));
            setBorderPen(QPen(QColor(203, 210, 233, 128), 0.7));
        }
    };

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

    m_toolTipItem = NULL;
    m_autoHideToolTip = true;
    m_sliderUnderMouse = false;
    m_toolTipUnderMouse = false;

    setToolTipItem(new QnSliderToolTipItem(this));
    setAcceptHoverEvents(true);

    m_animationListener.reset(new QnToolTipSliderAnimationListener(this));
    registerAnimation(m_animationListener.data());

    updateToolTipPosition();
    updateToolTipVisibility();
}

QnToolTipSlider::~QnToolTipSlider() {
    m_hideTimer.stop();
}

QnToolTipItem *QnToolTipSlider::toolTipItem() const {
    return m_toolTipItem;
}

void QnToolTipSlider::setToolTipItem(QnToolTipItem *toolTipItem) {
    qreal opacity = 0.0;
    if(m_toolTipItem) {
        opacity = m_toolTipItem->opacity();
        delete m_toolTipItem;
    }

    m_toolTipItem = toolTipItem;
    if(m_toolTipItem) {
        m_toolTipItem->setParent(this); /* Claim ownership, but not in graphics item sense. */
        m_toolTipItem->setFocusProxy(this);
        m_toolTipItem->setOpacity(opacity);
        m_toolTipItem->setAcceptHoverEvents(true);
        m_toolTipItem->installEventFilter(this);
        m_toolTipItem->setFlag(ItemIgnoresParentOpacity, true);

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
    opacityAnimator(m_toolTipItem, 1.0)->animateTo(0.0);
}

void QnToolTipSlider::showToolTip() {
    opacityAnimator(m_toolTipItem, 1.0)->animateTo(1.0);
}

void QnToolTipSlider::updateToolTipVisibility() {
    if(!m_toolTipItem)
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
    if(!m_toolTipItem)
        return;

    m_toolTipItem->setOpacity(effectiveOpacity());
}

void QnToolTipSlider::updateToolTipText() {
    if(!m_toolTipItem)
        return;

    m_toolTipItem->setText(toolTip());
}

void QnToolTipSlider::updateToolTipPosition() {
    if(!m_toolTipItem)
        return;

    QStyleOptionSlider opt;
    initStyleOption(&opt);
    QRect handleRect = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this);

    qreal x = positionFromValue(sliderPosition()).x() + handleRect.width() / 2.0;
    qreal y = handleRect.top();
    
    m_toolTipItem->setPos(m_toolTipItem->mapToParent(m_toolTipItem->mapFromItem(this, x, y)));
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnToolTipSliderAnimationListener::tick(int) {
    /* Unfortunately, there is no notification for a change in effective opacity,
     * so we have to track it in animation handler (which gets invoked before the
     * paint event). */
    m_slider->updateToolTipOpacity();
}

bool QnToolTipSlider::eventFilter(QObject *target, QEvent *event) {
    if(target == m_toolTipItem) {
        QGraphicsSceneMouseEvent *e = static_cast<QGraphicsSceneMouseEvent *>(event);

        switch(event->type()) {
        case QEvent::GraphicsSceneMousePress:
            if(e->button() == Qt::LeftButton) {
                setSliderDown(true);
                m_dragOffset = positionFromValue(sliderPosition()) - m_toolTipItem->mapToItem(this, e->pos());
                e->accept();
                return true;
            }
            break;
        case QEvent::GraphicsSceneMouseMove:
            if(isSliderDown()) {
                qint64 pos = valueFromPosition(m_toolTipItem->mapToItem(this, e->pos()) + m_dragOffset);
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

    if(m_toolTipItem) {
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

