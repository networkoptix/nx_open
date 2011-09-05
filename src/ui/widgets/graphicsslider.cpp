#include "graphicsslider.h"

#ifndef QT_NO_ACCESSIBILITY
#include <QtGui/QAccessible>
#endif
#include <QtGui/QApplication>
#include <QtGui/QGraphicsSceneEvent>
#include <QtGui/QPainter>
#include <QtGui/QStyle>
#include <QtGui/QStyleOption>

#include "abstractgraphicsslider_p.h"

class GraphicsSliderPrivate : public AbstractGraphicsSliderPrivate
{
    Q_DECLARE_PUBLIC(GraphicsSlider)

public:
    QStyle::SubControl pressedControl;
    int tickInterval;
    GraphicsSlider::TickPosition tickPosition;
    int clickOffset;

    void init();
    int pixelPosToRangeValue(int pos) const;
    inline int pick(const QPoint &pt) const
    { return orientation == Qt::Horizontal ? pt.x() : pt.y(); }

    QStyle::SubControl newHoverControl(const QPoint &pos);
    void updateHoverControl(const QPoint &pos);
    QStyle::SubControl hoverControl;
    QRect hoverRect;
};

void GraphicsSliderPrivate::init()
{
    Q_Q(GraphicsSlider);
    pressedControl = QStyle::SC_None;
    tickInterval = 0;
    tickPosition = GraphicsSlider::NoTicks;
    hoverControl = QStyle::SC_None;
    q->setFocusPolicy(Qt::FocusPolicy(q->style()->styleHint(QStyle::SH_Button_FocusPolicy)));
    QSizePolicy sp(QSizePolicy::Expanding, QSizePolicy::Fixed, QSizePolicy::Slider);
    if (orientation == Qt::Vertical)
        sp.transpose();
    q->setSizePolicy(sp);
//    q->setAttribute(Qt::WA_WState_OwnSizePolicy, false);
}

int GraphicsSliderPrivate::pixelPosToRangeValue(int pos) const
{
    Q_Q(const GraphicsSlider);
    QStyleOptionSlider opt;
    q->initStyleOption(&opt);
    QRect gr = q->style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove);
    QRect sr = q->style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle);
    int sliderMin, sliderMax, sliderLength;

    if (orientation == Qt::Horizontal) {
        sliderLength = sr.width();
        sliderMin = gr.x();
        sliderMax = gr.right() - sliderLength + 1;
    } else {
        sliderLength = sr.height();
        sliderMin = gr.y();
        sliderMax = gr.bottom() - sliderLength + 1;
    }
    return QStyle::sliderValueFromPosition(minimum, maximum, pos - sliderMin,
                                           sliderMax - sliderMin, opt.upsideDown);
}

void GraphicsSliderPrivate::updateHoverControl(const QPoint &pos)
{
    Q_Q(GraphicsSlider);
    QRect lastHoverRect = hoverRect;
    QStyle::SubControl lastHoverControl = hoverControl;
    if (lastHoverControl != newHoverControl(pos)) {
        q->update(lastHoverRect);
        q->update(hoverRect);
    }
}

QStyle::SubControl GraphicsSliderPrivate::newHoverControl(const QPoint &pos)
{
    Q_Q(GraphicsSlider);
    QStyleOptionSlider opt;
    q->initStyleOption(&opt);
    opt.subControls = QStyle::SC_All;
    QRect handleRect = q->style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle);
    QRect grooveRect = q->style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove);
    QRect tickmarksRect = q->style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderTickmarks);

    if (handleRect.contains(pos)) {
        hoverRect = handleRect;
        hoverControl = QStyle::SC_SliderHandle;
    } else if (grooveRect.contains(pos)) {
        hoverRect = grooveRect;
        hoverControl = QStyle::SC_SliderGroove;
    } else if (tickmarksRect.contains(pos)) {
        hoverRect = tickmarksRect;
        hoverControl = QStyle::SC_SliderTickmarks;
    } else {
        hoverRect = QRect();
        hoverControl = QStyle::SC_None;
    }

    return hoverControl;
}

/*!
    \class GraphicsSlider
    \brief The GraphicsSlider widget provides a vertical or horizontal slider.

    \ingroup basicwidgets


    The slider is the classic widget for controlling a bounded value.
    It lets the user move a slider handle along a horizontal or vertical
    groove and translates the handle's position into an integer value
    within the legal range.

    GraphicsSlider has very few of its own functions; most of the functionality is in
    AbstractGraphicsSlider. The most useful functions are setValue() to set
    the slider directly to some value; triggerAction() to simulate
    the effects of clicking (useful for shortcut keys);
    setSingleStep(), setPageStep() to set the steps; and setMinimum()
    and setMaximum() to define the range of the scroll bar.

    GraphicsSlider provides methods for controlling tickmarks.  You can use
    setTickPosition() to indicate where you want the tickmarks to be,
    setTickInterval() to indicate how many of them you want. the
    currently set tick position and interval can be queried using the
    tickPosition() and tickInterval() functions, respectively.

    GraphicsSlider inherits a comprehensive set of signals:
    \table
    \header \o Signal \o Description
    \row \o \l valueChanged()
    \o Emitted when the slider's value has changed. The tracking()
       determines whether this signal is emitted during user
       interaction.
    \row \o \l sliderPressed()
    \o Emitted when the user starts to drag the slider.
    \row \o \l sliderMoved()
    \o Emitted when the user drags the slider.
    \row \o \l sliderReleased()
    \o Emitted when the user releases the slider.
    \endtable

    GraphicsSlider only provides integer ranges. Note that although
    GraphicsSlider handles very large numbers, it becomes difficult for users
    to use a slider accurately for very large ranges.

    A slider accepts focus on Tab and provides both a mouse wheel and a
    keyboard interface. The keyboard interface is the following:

    \list
        \o Left/Right move a horizontal slider by one single step.
        \o Up/Down move a vertical slider by one single step.
        \o PageUp moves up one page.
        \o PageDown moves down one page.
        \o Home moves to the start (mininum).
        \o End moves to the end (maximum).
    \endlist

    \table 100%
    \row \o \inlineimage macintosh-slider.png Screenshot of a Macintosh slider
         \o A slider shown in the \l{Macintosh Style Widget Gallery}{Macintosh widget style}.
    \row \o \inlineimage windows-slider.png Screenshot of a Windows XP slider
         \o A slider shown in the \l{Windows XP Style Widget Gallery}{Windows XP widget style}.
    \row \o \inlineimage plastique-slider.png Screenshot of a Plastique slider
         \o A slider shown in the \l{Plastique Style Widget Gallery}{Plastique widget style}.
    \endtable

    \sa QScrollBar, QSpinBox, QDial, {fowler}{GUI Design Handbook: Slider}, {Sliders Example}
*/


/*!
    \enum GraphicsSlider::TickPosition

    This enum specifies where the tick marks are to be drawn relative
    to the slider's groove and the handle the user moves.

    \value NoTicks Do not draw any tick marks.
    \value TicksBothSides Draw tick marks on both sides of the groove.
    \value TicksAbove Draw tick marks above the (horizontal) slider
    \value TicksBelow Draw tick marks below the (horizontal) slider
    \value TicksLeft Draw tick marks to the left of the (vertical) slider
    \value TicksRight Draw tick marks to the right of the (vertical) slider
*/


/*!
    Constructs a vertical slider with the given \a parent.
*/
GraphicsSlider::GraphicsSlider(QGraphicsItem *parent)
    : AbstractGraphicsSlider(*new GraphicsSliderPrivate, parent)
{
    Q_D(GraphicsSlider);
    d->orientation = Qt::Horizontal;
    d->init();
}

/*!
    Constructs a slider with the given \a parent. The \a orientation
    parameter determines whether the slider is horizontal or vertical;
    the valid values are Qt::Vertical and Qt::Horizontal.
*/

GraphicsSlider::GraphicsSlider(Qt::Orientation orientation, QGraphicsItem *parent)
    : AbstractGraphicsSlider(*new GraphicsSliderPrivate, parent)
{
    Q_D(GraphicsSlider);
    d->orientation = orientation;
    d->init();
}

/*!
    Destroys this slider.
*/
GraphicsSlider::~GraphicsSlider()
{
}

/*!
    \reimp
*/
void GraphicsSlider::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option)

    Q_D(GraphicsSlider);

    QStyleOptionSlider opt;
    initStyleOption(&opt);
    opt.subControls = QStyle::SC_SliderGroove | QStyle::SC_SliderHandle;
    if (d->tickPosition != NoTicks)
        opt.subControls |= QStyle::SC_SliderTickmarks;
    if (d->pressedControl) {
        opt.activeSubControls = d->pressedControl;
        opt.state |= QStyle::State_Sunken;
    } else {
        opt.activeSubControls = d->hoverControl;
    }

    style()->drawComplexControl(QStyle::CC_Slider, &opt, painter, widget);
}

/*!
    \reimp
*/

bool GraphicsSlider::event(QEvent *event)
{
    Q_D(GraphicsSlider);

    switch(event->type()) {
    case QEvent::GraphicsSceneHoverEnter:
    case QEvent::GraphicsSceneHoverMove:
    case QEvent::GraphicsSceneHoverLeave:
        d->updateHoverControl(static_cast<QGraphicsSceneHoverEvent *>(event)->pos().toPoint());
        break;

    default:
        break;
    }

    return AbstractGraphicsSlider::event(event);
}

/*!
    \reimp
*/
void GraphicsSlider::mousePressEvent(QGraphicsSceneMouseEvent *ev)
{
    Q_D(GraphicsSlider);
    if (d->maximum == d->minimum || (ev->buttons() ^ ev->button())) {
        ev->ignore();
        return;
    }
#ifdef QT_KEYPAD_NAVIGATION
    if (QApplication::keypadNavigationEnabled())
        setEditFocus(true);
#endif
    ev->accept();
    if ((ev->button() & style()->styleHint(QStyle::SH_Slider_AbsoluteSetButtons)) != 0) {
        QStyleOptionSlider opt;
        initStyleOption(&opt);
        const QRect sliderRect = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle);
        const QPoint center = sliderRect.center() - sliderRect.topLeft();
        // to take half of the slider off for the setSliderPosition call we use the center - topLeft

        setSliderPosition(d->pixelPosToRangeValue(d->pick(ev->pos().toPoint() - center)));
        triggerAction(SliderMove);
        setRepeatAction(SliderNoAction);
        d->pressedControl = QStyle::SC_SliderHandle;
        update();
    } else if ((ev->button() & style()->styleHint(QStyle::SH_Slider_PageSetButtons)) != 0) {
        QStyleOptionSlider opt;
        initStyleOption(&opt);
        d->pressedControl = style()->hitTestComplexControl(QStyle::CC_Slider, &opt, ev->pos().toPoint());
        SliderAction action = SliderNoAction;
        if (d->pressedControl == QStyle::SC_SliderGroove) {
            const QRect sliderRect = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle);
            int pressValue = d->pixelPosToRangeValue(d->pick(ev->pos().toPoint() - sliderRect.center() + sliderRect.topLeft()));
            d->pressValue = pressValue;
            if (pressValue > d->value)
                action = SliderPageStepAdd;
            else if (pressValue < d->value)
                action = SliderPageStepSub;
            if (action) {
                triggerAction(action);
                setRepeatAction(action);
            }
        }
    } else {
        ev->ignore();
        return;
    }

    if (d->pressedControl == QStyle::SC_SliderHandle) {
        QStyleOptionSlider opt;
        initStyleOption(&opt);
        setRepeatAction(SliderNoAction);
        QRect sr = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle);
        d->clickOffset = d->pick(ev->pos().toPoint() - sr.topLeft());
        update(sr);
        setSliderDown(true);
    }
}

/*!
    \reimp
*/
void GraphicsSlider::mouseMoveEvent(QGraphicsSceneMouseEvent *ev)
{
    Q_D(GraphicsSlider);
    if (d->pressedControl != QStyle::SC_SliderHandle) {
        ev->ignore();
        return;
    }

    ev->accept();
    int newPosition = d->pixelPosToRangeValue(d->pick(ev->pos().toPoint()) - d->clickOffset);
    QStyleOptionSlider opt;
    initStyleOption(&opt);
    setSliderPosition(newPosition);
}

/*!
    \reimp
*/
void GraphicsSlider::mouseReleaseEvent(QGraphicsSceneMouseEvent *ev)
{
    Q_D(GraphicsSlider);
    if (d->pressedControl == QStyle::SC_None || ev->buttons()) {
        ev->ignore();
        return;
    }

    ev->accept();
    QStyle::SubControl oldPressed = QStyle::SubControl(d->pressedControl);
    d->pressedControl = QStyle::SC_None;
    setRepeatAction(SliderNoAction);
    if (oldPressed == QStyle::SC_SliderHandle)
        setSliderDown(false);
    QStyleOptionSlider opt;
    initStyleOption(&opt);
    opt.subControls = oldPressed;
    update(style()->subControlRect(QStyle::CC_Slider, &opt, oldPressed));
}

/*!
    Initialize \a option with the values from this GraphicsSlider. This method
    is useful for subclasses when they need a QStyleOptionSlider, but don't want
    to fill in all the information themselves.

    \sa QStyleOption::initFrom()
*/
void GraphicsSlider::initStyleOption(QStyleOptionSlider *option) const
{
    AbstractGraphicsSlider::initStyleOption(option);

    Q_D(const GraphicsSlider);

    option->subControls = QStyle::SC_None;
    option->activeSubControls = QStyle::SC_None;
    option->tickPosition = (QSlider::TickPosition)d->tickPosition;
    option->tickInterval = d->tickInterval;
}

/*!
    \reimp
*/
QSizeF GraphicsSlider::sizeHint(Qt::SizeHint which, const QSizeF &constraint) const
{
    Q_D(const GraphicsSlider);

    QSizeF sh;
    switch (which) {
    case Qt::MinimumSize:
    case Qt::PreferredSize:
    {
        const int SliderLength = 84, TickSpace = 5;
        QStyleOptionSlider opt;
        initStyleOption(&opt);
        int thick = style()->pixelMetric(QStyle::PM_SliderThickness, &opt);
        if (d->tickPosition & TicksAbove)
            thick += TickSpace;
        if (d->tickPosition & TicksBelow)
            thick += TickSpace;
        int w = SliderLength, h = thick;
        if (d->orientation == Qt::Vertical) {
            w = thick;
            h = SliderLength;
        }
        sh = QSizeF(style()->sizeFromContents(QStyle::CT_Slider, &opt, QSize(w, h)).expandedTo(QApplication::globalStrut()));

        if (which == Qt::MinimumSize) {
            int length = style()->pixelMetric(QStyle::PM_SliderLength, &opt);
            if (d->orientation == Qt::Horizontal)
                sh.setWidth(length);
            else
                sh.setHeight(length);
        }
    }
        break;
    case Qt::MaximumSize:
    default:
        sh = AbstractGraphicsSlider::sizeHint(which, constraint);
        break;
    }
    return sh;
}

/*!
    \property GraphicsSlider::tickPosition
    \brief the tickmark position for this slider

    The valid values are described by the GraphicsSlider::TickPosition enum.

    The default value is \l GraphicsSlider::NoTicks.

    \sa tickInterval
*/
void GraphicsSlider::setTickPosition(TickPosition position)
{
    Q_D(GraphicsSlider);
    d->tickPosition = position;
    updateGeometry();
    update();
}

GraphicsSlider::TickPosition GraphicsSlider::tickPosition() const
{
    return d_func()->tickPosition;
}


/*!
    \property GraphicsSlider::tickInterval
    \brief the interval between tickmarks

    This is a value interval, not a pixel interval. If it is 0, the
    slider will choose between singleStep() and pageStep().

    The default value is 0.

    \sa tickPosition, lineStep(), pageStep()
*/
int GraphicsSlider::tickInterval() const
{
    return d_func()->tickInterval;
}

void GraphicsSlider::setTickInterval(int ts)
{
    d_func()->tickInterval = qMax(0, ts);
    update();
}

/*!
    \internal

    Returns the style option for slider.
*/
/*Q_GUI_EXPORT QStyleOptionSlider qt_qsliderStyleOption(GraphicsSlider *slider)
{
    QStyleOptionSlider sliderOption;
    slider->initStyleOption(&sliderOption);
    return sliderOption;
}*/
