#include "graphicsframe.h"
#include "graphicsframe_p.h"

#include <QtGui/QApplication>
#include <QtGui/QGraphicsSceneEvent>
#include <QtGui/QPainter>
#include <QtGui/QStyle>
#include <QtGui/QStyleOption>

void GraphicsFramePrivate::updateStyledFrameWidths()
{
    Q_Q(const GraphicsFrame);
    QStyleOptionFrameV3 opt;
    q->initStyleOption(&opt);
    opt.lineWidth = lineWidth;
    opt.midLineWidth = midLineWidth;

    const QRect cr = q->style()->subElementRect(QStyle::SE_ShapedFrameContents, &opt, 0);
    leftFrameWidth = cr.left() - opt.rect.left();
    topFrameWidth = cr.top() - opt.rect.top();
    rightFrameWidth = opt.rect.right() - cr.right(),
    bottomFrameWidth = opt.rect.bottom() - cr.bottom();
    frameWidth = qMax(qMax(leftFrameWidth, rightFrameWidth), qMax(topFrameWidth, bottomFrameWidth));
}

void GraphicsFramePrivate::updateFrameWidth()
{
    Q_Q(GraphicsFrame);
    const QRectF fr = q->frameRect();
    updateStyledFrameWidths();
    q->setFrameRect(fr);
}


/*!
    \class GraphicsFrame
    \brief The GraphicsFrame class is the base class of widgets that can have a frame.

    \ingroup abstractwidgets


    QMenu uses this to "raise" the menu above the surrounding
    screen. QProgressBar has a "sunken" look. QLabel has a flat look.
    The frames of widgets like these can be changed.

    The GraphicsFrame class can also be used directly for creating simple
    placeholder frames without any contents.

    The frame style is specified by a \l{GraphicsFrame::Shape}{frame shape} and
    a \l{GraphicsFrame::Shadow}{shadow style} that is used to visually separate
    the frame from surrounding widgets. These properties can be set
    together using the setFrameStyle() function and read with frameStyle().

    The frame shapes are \l NoFrame, \l Box, \l Panel, \l StyledPanel,
    HLine and \l VLine; the shadow styles are \l Plain, \l Raised and \l Sunken.

    A frame widget has three attributes that describe the thickness of the
    border: \l lineWidth, \l midLineWidth, and \l frameWidth.

    \list
    \o The line width is the width of the frame border. It can be modified
       to customize the frame's appearance.

    \o The mid-line width specifies the width of an extra line in the
       middle of the frame, which uses a third color to obtain a special
       3D effect. Notice that a mid-line is only drawn for \l Box, \l
       HLine and \l VLine frames that are raised or sunken.

    \o The frame width is determined by the frame style, and the frameWidth()
       function is used to obtain the value defined for the style used.
    \endlist

    The margin between the frame and the contents of the frame can be
    customized with the QGraphicsWidget::setContentsMargins() function.
*/

/*!
    \enum GraphicsFrame::Shape

    This enum type defines the shapes of frame available.

    \value NoFrame      draws nothing
    \value Box          draws a box around its contents
    \value Panel        draws a panel to make the contents appear raised or sunken
    \value StyledPanel  draws a rectangular panel with a look that depends on the
                        current GUI style. It can be raised or sunken.
    \value HLine        draws a horizontal line that frames nothing (useful as separator)
    \value VLine        draws a vertical line that frames nothing (useful as separator)
    \value WinPanel     draws a rectangular panel that can be raised or sunken
                        like those in Windows 2000. Specifying this shape sets
                        the line width to 2 pixels. WinPanel is provided for compatibility.
                        For GUI style independence we recommend using StyledPanel instead.
    \omitvalue GroupBoxPanel
    \omitvalue ToolBarPanel
    \omitvalue MenuBarPanel
    \omitvalue PopupPanel
    \omitvalue LineEditPanel
    \omitvalue TabWidgetPanel

    When it does not call QStyle, Shape interacts with GraphicsFrame::Shadow,
    the lineWidth() and the midLineWidth() to create the total result.
    See the picture of the frames in the main class documentation.

    \sa GraphicsFrame::Shadow, GraphicsFrame::style(), QStyle::drawPrimitive()
*/

/*!
    \enum GraphicsFrame::Shadow

    This enum type defines the types of shadow that are used to give
    a 3D effect to frames.

    \value Plain   the frame and contents appear level with the surroundings;
                   draws using the palette QPalette::WindowText color (without any 3D effect)
    \value Raised  the frame and contents appear raised; draws a 3D raised line
                   using the light and dark colors of the current color group
    \value Sunken  the frame and contents appear sunken; draws a 3D sunken line
                   using the light and dark colors of the current color group

    Shadow interacts with GraphicsFrame::Shape, the lineWidth() and the
    midLineWidth(). See the picture of the frames in the main class
    documentation.

    \sa GraphicsFrame::Shape, lineWidth(), midLineWidth()
*/

/*!
    \enum GraphicsFrame::StyleMask

    This enum defines two constants that can be used to extract the
    two components of frameStyle():

    \value Shadow_Mask  The \l Shadow part of frameStyle()
    \value Shape_Mask   The \l Shape part of frameStyle()
    \omitvalue MShadow
    \omitvalue MShape

    Normally, you don't need to use these, since frameShadow() and
    frameShape() already extract the \l Shadow and the \l Shape parts
    of frameStyle().

    \sa frameStyle(), setFrameStyle()
*/

/*!
    Constructs a frame widget with frame style \l NoFrame and a 1-pixel frame width.
*/
GraphicsFrame::GraphicsFrame(QGraphicsItem *parent, Qt::WindowFlags f): 
    GraphicsWidget(*new GraphicsFramePrivate(), parent, f)
{
}

/*!
    \internal
*/
GraphicsFrame::GraphicsFrame(GraphicsFramePrivate &dd, QGraphicsItem *parent, Qt::WindowFlags f): 
    GraphicsWidget(dd, parent, f)
{
}

/*!
    Destroys the frame.
*/
GraphicsFrame::~GraphicsFrame()
{
}

/*!
    Returns the frame style.

    The default value is GraphicsFrame::Plain.

    \sa setFrameStyle(), frameShape(), frameShadow()
*/
int GraphicsFrame::frameStyle() const
{
    return d_func()->frameStyle;
}

/*!
    Sets the frame style to \a style.

    The \a style is the bitwise OR between a frame shape and a frame shadow style.
    See the picture of the frames in the main class documentation.

    The frame shapes are given in \l{GraphicsFrame::Shape} and the shadow
    styles in \l{GraphicsFrame::Shadow}.

    If a mid-line width greater than 0 is specified, an additional line is drawn
    for \l Raised or \l Sunken \l Box, \l HLine, and \l VLine frames.
    The mid-color of the current color group is used for drawing middle lines.

    \sa frameStyle()
*/
void GraphicsFrame::setFrameStyle(int style)
{
    Q_D(GraphicsFrame);

    QSizePolicy sp;
    switch (style & Shape_Mask) {
    case HLine:
        sp = QSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed, QSizePolicy::Line);
        break;
    case VLine:
        sp = QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum, QSizePolicy::Line);
        break;
    default:
        sp = QSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred, QSizePolicy::Frame);
    }
    setSizePolicy(sp);

    d->frameStyle = (short)style;
    d->updateFrameWidth();
    update();
}

/*!
    \property GraphicsFrame::frameShape
    \brief the frame shape value from the frame style

    \sa frameStyle(), frameShadow()
*/
GraphicsFrame::Shape GraphicsFrame::frameShape() const
{
    Q_D(const GraphicsFrame);
    return static_cast<Shape>(d->frameStyle & Shape_Mask);
}

void GraphicsFrame::setFrameShape(GraphicsFrame::Shape shape)
{
    Q_D(GraphicsFrame);
    setFrameStyle((d->frameStyle & Shadow_Mask) | shape);
}

/*!
    \property GraphicsFrame::frameShadow
    \brief the frame shadow value from the frame style

    \sa frameStyle(), frameShape()
*/
GraphicsFrame::Shadow GraphicsFrame::frameShadow() const
{
    Q_D(const GraphicsFrame);
    return static_cast<Shadow>(d->frameStyle & Shadow_Mask);
}

void GraphicsFrame::setFrameShadow(GraphicsFrame::Shadow shadow)
{
    Q_D(GraphicsFrame);
    setFrameStyle((d->frameStyle & Shape_Mask) | shadow);
}

/*!
    \property GraphicsFrame::frameWidth
    \brief the width of the frame that is drawn.

    Note that the frame width depends on the \l{GraphicsFrame::setFrameStyle()}{frame style},
    not only the line width and the mid-line width. For example, the style specified
    by \l NoFrame always has a frame width of 0, whereas the style \l Panel has a
    frame width equivalent to the line width.

    \sa lineWidth(), midLineWidth(), frameStyle()
*/
int GraphicsFrame::frameWidth() const
{
    return d_func()->frameWidth;
}

/*!
    \property GraphicsFrame::lineWidth
    \brief the line width

    Note that the \e total line width for frames used as separators
    (\l HLine and \l VLine) is specified by \l frameWidth.

    The default value is 1.

    \sa midLineWidth, frameWidth
*/
int GraphicsFrame::lineWidth() const
{
    return d_func()->lineWidth;
}

void GraphicsFrame::setLineWidth(int width)
{
    Q_D(GraphicsFrame);
    if (short(width) == d->lineWidth)
        return;
    d->lineWidth = short(width);
    d->updateFrameWidth();
}

/*!
    \property GraphicsFrame::midLineWidth
    \brief the width of the mid-line

    The default value is 0.

    \sa lineWidth, frameWidth
*/
int GraphicsFrame::midLineWidth() const
{
    return d_func()->midLineWidth;
}

void GraphicsFrame::setMidLineWidth(int width)
{
    Q_D(GraphicsFrame);
    if (short(width) == d->midLineWidth)
        return;
    d->midLineWidth = short(width);
    d->updateFrameWidth();
}

/*!
    \property GraphicsFrame::frameRect
    \brief the frame's rectangle

    The frame's rectangle is the rectangle the frame is drawn in. By
    default, this is the entire widget. Setting the rectangle does
    does \e not cause a widget update. The frame rectangle is
    automatically adjusted when the widget changes size.

    If you set the rectangle to a null rectangle (for example,
    QRect(0, 0, 0, 0)), then the resulting frame rectangle is
    equivalent to the \link QGraphicsWidget::rect() widget rectangle\endlink.
*/
QRectF GraphicsFrame::frameRect() const
{
    Q_D(const GraphicsFrame);
    return contentsRect().adjusted(-d->leftFrameWidth, -d->topFrameWidth, d->rightFrameWidth, d->bottomFrameWidth);
}

void GraphicsFrame::setFrameRect(const QRectF &r)
{
    Q_D(GraphicsFrame);
    QRectF cr = r.isValid() ? r : rect();
    cr.adjust(d->leftFrameWidth, d->topFrameWidth, -d->rightFrameWidth, -d->bottomFrameWidth);
    setContentsMargins(cr.left(), cr.top(), rect().right() - cr.right(), rect().bottom() - cr.bottom());
}

/*!
    \reimp
*/
void GraphicsFrame::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option)

    Q_D(GraphicsFrame);

    QStyleOptionFrameV3 opt;
    initStyleOption(&opt);

    switch (d->frameStyle & GraphicsFrame::Shape_Mask) {
    case GraphicsFrame::Box:
    case GraphicsFrame::HLine:
    case GraphicsFrame::VLine:
    case GraphicsFrame::StyledPanel:
    case GraphicsFrame::Panel:
        opt.lineWidth = d->lineWidth;
        opt.midLineWidth = d->midLineWidth;
        break;
    default:
        // most frame styles do not handle customized line and midline widths (see updateFrameWidth())
        opt.lineWidth = d->frameWidth;
        break;
    }

    switch (d->frameStyle & GraphicsFrame::Shadow_Mask) {
    case Sunken:
        opt.state |= QStyle::State_Sunken;
        break;
    case Raised:
        opt.state |= QStyle::State_Raised;
        break;
    default:
        break;
    }

    style()->drawControl(QStyle::CE_ShapedFrame, &opt, painter, widget);
}

void GraphicsFrame::initStyleOption(QStyleOption *option) const
{
    GraphicsWidget::initStyleOption(option);

    if (QStyleOptionFrame *frameOption = qstyleoption_cast<QStyleOptionFrame *>(option)) {
        Q_D(const GraphicsFrame);
        frameOption->rect = frameRect().toRect(); // truncation!
        //frameOption->lineWidth = d->lineWidth;
        //frameOption->midLineWidth = d->midLineWidth;
        if (QStyleOptionFrameV3 *frameOptionV3 = qstyleoption_cast<QStyleOptionFrameV3 *>(frameOption))
            frameOptionV3->frameShape = QFrame::Shape(int(frameOptionV3->frameShape) | (d->frameStyle & Shape_Mask));
    }
}

/*!
    \reimp
*/
QSizeF GraphicsFrame::sizeHint(Qt::SizeHint which, const QSizeF &constraint) const
{
    Q_D(const GraphicsFrame);
    // Returns a size hint for the frame - for HLine and VLine shapes,
    // this is stretchable one way and 3 pixels wide the other.
    // For other shapes, GraphicsWidget::sizeHint() is used.
    switch (d->frameStyle & Shape_Mask) {
    case HLine: return QSizeF(-1, 3);
    case VLine: return QSizeF(3, -1);
    default: break;
    }

    return GraphicsWidget::sizeHint(which, constraint);
}

/*!
    \reimp
*/
bool GraphicsFrame::event(QEvent *event)
{
    if (event->type() == QEvent::ParentChange)
        d_func()->updateFrameWidth();
    bool result = GraphicsWidget::event(event);
    //this has to be done after the widget has been polished
    if (event->type() == QEvent::Polish)
        d_func()->updateFrameWidth();
    return result;
}

/*!
    \reimp
*/
void GraphicsFrame::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::StyleChange)
        d_func()->updateFrameWidth();
#ifdef Q_WS_MAC
    else if (event->type() == QEvent::MacSizeChange)
        d_func()->updateFrameWidth();
#endif
    GraphicsWidget::changeEvent(event);
}
