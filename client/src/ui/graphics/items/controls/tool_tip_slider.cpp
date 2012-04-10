#include "tool_tip_slider.h"

#include <QtGui/QStyleOptionSlider>
#include <QtGui/QGraphicsSceneMouseEvent>

#include <ui/animation/widget_opacity_animator.h>
#include <ui/style/noptix_style.h>

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

    qreal horizontalCenter(const QRect &rect) {
        /* Note that QRect::right != QRect::left + QRect::width. */
        return (2 * rect.left() + rect.width()) / 2.0;
    }

    const int toolTipHideDelay = 2500;

} // anonymous namespace


QnToolTipSlider::QnToolTipSlider(QGraphicsItem *parent):
    base_type(Qt::Horizontal, parent),
    m_toolTipItem(NULL),
    m_autoHideToolTip(true),
    m_sliderUnderMouse(false),
    m_toolTipUnderMouse(false)
{
    setToolTipItem(new QnSliderToolTipItem());
    setAcceptHoverEvents(true);
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
        m_toolTipItem->setParentItem(this);
        m_toolTipItem->setOpacity(opacity);
        m_toolTipItem->setText(toolTip());
        m_toolTipItem->setAcceptHoverEvents(true);
        m_toolTipItem->installSceneEventFilter(this);
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


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
bool QnToolTipSlider::sceneEventFilter(QGraphicsItem *target, QEvent *event) {
    if(target == m_toolTipItem) {
        QGraphicsSceneMouseEvent *e = static_cast<QGraphicsSceneMouseEvent *>(event);

        switch(event->type()) {
        case QEvent::GraphicsSceneMousePress:
            if(e->button() == Qt::LeftButton) {
                setSliderDown(true);

                QStyleOptionSlider opt;
                initStyleOption(&opt);
                const QRect handleRect = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle);

                m_dragOffset = handleRect.left() - target->mapToItem(this, e->pos()).x();

                e->accept();
                return true;
            }
            break;
        case QEvent::GraphicsSceneMouseMove:
            if(isSliderDown()) {
                QStyleOptionSlider opt;
                initStyleOption(&opt);
                QRect grooveRect = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove);
                QRect handleRect = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle);

                int sliderMin = grooveRect.x();
                int sliderMax = grooveRect.right() - handleRect.width() + 1;

                qint64 pos = QnNoptixStyle::sliderValueFromPosition(minimum(), maximum(), target->mapToItem(this, e->pos()).x() + m_dragOffset - sliderMin, sliderMax - sliderMin, opt.upsideDown);
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
        return base_type::sceneEventFilter(target, event);
    }
}

void QnToolTipSlider::sliderChange(SliderChange change) {
    base_type::sliderChange(change);

    if(change == SliderValueChange && m_toolTipItem) {
        if(m_autoHideToolTip)
            m_hideTimer.start(toolTipHideDelay, this);

        QStyleOptionSlider opt;
        initStyleOption(&opt);
        
        const QRect handleRect = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle);
        m_toolTipItem->setPos(horizontalCenter(handleRect), handleRect.top());

        updateToolTipVisibility();
    }
}

QVariant QnToolTipSlider::itemChange(GraphicsItemChange change, const QVariant &value) {
    QVariant result = base_type::itemChange(change, value);

    if(change == ItemToolTipHasChanged && m_toolTipItem)
        m_toolTipItem->setText(toolTip());

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


