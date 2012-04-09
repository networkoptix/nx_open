#include "tool_tip_slider.h"

#include <QtGui/QStyleOptionSlider>

#include <ui/animation/widget_opacity_animator.h>

#include "tool_tip_item.h"


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


QnToolTipSlider::QnToolTipSlider(QGraphicsItem *parent):
    base_type(parent),
    m_toolTipItem(NULL)
{
    setToolTipItem(new QnSliderToolTipItem());
}

QnToolTipSlider::QnToolTipSlider(Qt::Orientation orientation, QGraphicsItem *parent):
    base_type(orientation, parent),
    m_toolTipItem(NULL)
{
    setToolTipItem(new QnSliderToolTipItem());
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
    }
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnToolTipSlider::sliderChange(SliderChange change) {
    base_type::sliderChange(change);

    if(change == SliderValueChange && m_toolTipItem) {
        m_hideTimer.start(2500, this);

        QStyleOptionSlider opt;
        initStyleOption(&opt);
        
        /* Note that QRect::right != QRect::left + QRect::width. */
        const QRect sliderRect = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle);
        m_toolTipItem->setPos((2 * sliderRect.left() + sliderRect.width()) / 2.0, sliderRect.top());

        opacityAnimator(m_toolTipItem, 1.0)->animateTo(1.0);
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

    if(event->timerId() == m_hideTimer.timerId() && m_toolTipItem)
        opacityAnimator(m_toolTipItem, 1.0)->animateTo(0.0);
}



