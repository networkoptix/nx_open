#include "graphics_frame.h"
#include "graphics_frame_p.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QGraphicsSceneEvent>
#include <QtGui/QPainter>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleOption>

void GraphicsFramePrivate::updateStyledFrameWidths()
{
    Q_Q(const GraphicsFrame);
    QStyleOptionFrameV3 opt;
    q->initStyleOption(&opt);
    opt.lineWidth = lineWidth;
    opt.midLineWidth = midLineWidth;

    const QRect cr = q->style()->subElementRect(QStyle::SE_ShapedFrameContents, &opt, NULL);
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

GraphicsFrame::GraphicsFrame(QGraphicsItem *parent, Qt::WindowFlags f): 
    GraphicsWidget(*new GraphicsFramePrivate(), parent, f)
{
}

GraphicsFrame::GraphicsFrame(GraphicsFramePrivate &dd, QGraphicsItem *parent, Qt::WindowFlags f): 
    GraphicsWidget(dd, parent, f)
{
}

GraphicsFrame::~GraphicsFrame()
{
}

int GraphicsFrame::frameStyle() const
{
    return d_func()->frameStyle;
}

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

int GraphicsFrame::frameWidth() const
{
    return d_func()->frameWidth;
}

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

void GraphicsFrame::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
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

    style()->drawControl(QStyle::CE_ShapedFrame, &opt, painter, NULL);
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

void GraphicsFrame::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::StyleChange)
        d_func()->updateFrameWidth();
#ifdef Q_OS_MAC
    else if (event->type() == QEvent::MacSizeChange)
        d_func()->updateFrameWidth();
#endif
    GraphicsWidget::changeEvent(event);
}
