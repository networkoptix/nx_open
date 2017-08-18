#include "overlay_label_widget.h"

#include <QtGui/QPainter>
#include <QtGui/QMouseEvent>
#include <QtWidgets/QLabel>

namespace nx {
namespace client {
namespace desktop {
namespace ui {

namespace {

static constexpr qreal kBorderThickness = 2.0;

} // namespace

class OverlayLabelWidget::FrameWidget: public QWidget
{
public:
    using QWidget::QWidget;

protected:
    virtual void paintEvent(QPaintEvent* /*event*/) override
    {
        QPainter painter(this);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(palette().link(), kBorderThickness, Qt::DashLine));
        painter.drawRect(rect());
    }
};

struct OverlayLabelWidget::Private
{
    Private(OverlayLabelWidget* parent):
        label(new QLabel(parent)),
        frame(new FrameWidget(parent))
    {
        label->setAttribute(Qt::WA_TransparentForMouseEvents);
        frame->setAttribute(Qt::WA_TransparentForMouseEvents);
    }

    QLabel* label;
    QWidget* frame;
    bool borderVisible = true;
    qreal roundingRadius = 4.0;

    QPoint initialPos;
    bool dragging = false;
};

OverlayLabelWidget::OverlayLabelWidget(QWidget* parent):
    base_type(parent),
    d(new Private(this))
{
    updateLayout();
    updateCursor();

    if (parent)
        parent->installEventFilter(this);
}

QString OverlayLabelWidget::text() const
{
    return d->label->text();
}

void OverlayLabelWidget::setText(const QString& value)
{
    d->label->setText(value);
}

bool OverlayLabelWidget::wordWrap() const
{
    return d->label->wordWrap();
}

void OverlayLabelWidget::setWordWrap(bool value)
{
    d->label->setWordWrap(value);
}

QPixmap OverlayLabelWidget::pixmap() const
{
    return *d->label->pixmap();
}

void OverlayLabelWidget::setPixmap(const QPixmap& value)
{
    d->label->setPixmap(value);
}

bool OverlayLabelWidget::borderVisible() const
{
    return !d->frame->isHidden();
}

void OverlayLabelWidget::setBorderVisible(bool value)
{
    d->frame->setHidden(!value);
}

qreal OverlayLabelWidget::roundingRadius() const
{
    return d->roundingRadius;
}

void OverlayLabelWidget::setRoundingRadius(qreal value)
{
    if (qFuzzyIsNull(d->roundingRadius - value))
        return;

    d->roundingRadius = value;
    update();
}

void OverlayLabelWidget::updateLayout()
{
    QSize size = minimumSize().expandedTo(d->label->minimumSize())
        .expandedTo(d->label->minimumSizeHint());

    if (d->label->hasHeightForWidth())
        size.setHeight(d->label->heightForWidth(size.width()));

    resize(size);
    d->label->resize(size);
    d->frame->resize(size);
}

void OverlayLabelWidget::updateCursor()
{
    setCursor(d->dragging ? Qt::ClosedHandCursor : Qt::OpenHandCursor);
}

void OverlayLabelWidget::updatePosition(const QPoint& pos)
{
    int x = pos.x();
    int y = pos.y();

    if (auto parent = parentWidget())
    {
        const auto rect = parent->contentsRect();
        x = qMax(qMin(x, rect.width() - width()), rect.left());
        y = qMax(qMin(y, rect.height() - height()), rect.top());
    }

    move(x, y);
}

void OverlayLabelWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.setBrush(palette().window());
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(rect(), d->roundingRadius, d->roundingRadius);
}

void OverlayLabelWidget::mousePressEvent(QMouseEvent* event)
{
    d->initialPos = event->pos();
    d->dragging = true;
    updateCursor();
    event->accept();
}

void OverlayLabelWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (!d->dragging)
        return;

    const auto delta = event->pos() - d->initialPos;
    if (delta.isNull())
        return;

    updatePosition(pos() + delta);
    event->accept();
}

void OverlayLabelWidget::mouseReleaseEvent(QMouseEvent* event)
{
    d->dragging = false;
    updateCursor();
    event->accept();
}

bool OverlayLabelWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == parent() && event->type() == QEvent::Resize)
        updatePosition(pos());

    return base_type::eventFilter(watched, event);
}

bool OverlayLabelWidget::event(QEvent* event)
{
    switch (event->type())
    {
        case QEvent::ParentChange:
        {
            if (auto parent = parentWidget())
                parent->installEventFilter(this);
            break;
        }

        case QEvent::ParentAboutToChange:
        {
            if (auto parent = parentWidget())
                parent->removeEventFilter(this);
            break;
        }

        case QEvent::Resize:
        {
            updatePosition(pos());
            break;
        }

        case QEvent::Show:
        case QEvent::LayoutRequest:
        {
            updateLayout();
            break;
        }

        default:
            break;
    }

    return base_type::event(event);
}

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
