#include "graphics_scroll_area.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QGraphicsSceneWheelEvent>

class QnGraphicsScrollAreaPrivate: public QObject
{
    Q_DECLARE_PUBLIC(QnGraphicsScrollArea)
    QnGraphicsScrollArea* const q_ptr;

public:
    QnGraphicsScrollAreaPrivate(QnGraphicsScrollArea* parent);

    QGraphicsWidget* contentWidget = nullptr;
    qreal yOffset = 0.0;
    Qt::Alignment alignment = 0;
    int lineHeight = 0;

    void fitToBounds();
};

QnGraphicsScrollArea::QnGraphicsScrollArea(QGraphicsItem* parent):
    base_type(parent),
    d_ptr(new QnGraphicsScrollAreaPrivate(this))
{
    Q_D(QnGraphicsScrollArea);
    setAcceptedMouseButtons(Qt::NoButton);
    connect(this, &QnGraphicsScrollArea::heightChanged,
        d, &QnGraphicsScrollAreaPrivate::fitToBounds);
}

QnGraphicsScrollArea::~QnGraphicsScrollArea()
{
}

QGraphicsWidget* QnGraphicsScrollArea::contentWidget() const
{
    Q_D(const QnGraphicsScrollArea);
    return d->contentWidget;
}

void QnGraphicsScrollArea::setContentWidget(QGraphicsWidget* widget)
{
    Q_D(QnGraphicsScrollArea);
    if (d->contentWidget == widget)
        return;

    if (d->contentWidget)
    {
        disconnect(d->contentWidget, nullptr, this, nullptr);
        d->contentWidget->setParentItem(nullptr);
    }

    d->contentWidget = widget;
    d->contentWidget->setParentItem(this);

    connect(d->contentWidget, &QGraphicsWidget::heightChanged,
        d, &QnGraphicsScrollAreaPrivate::fitToBounds);

    d->fitToBounds();
}

Qt::Alignment QnGraphicsScrollArea::alignment() const
{
    Q_D(const QnGraphicsScrollArea);
    return d->alignment;
}

void QnGraphicsScrollArea::setAlignment(Qt::Alignment alignment)
{
    Q_D(QnGraphicsScrollArea);
    if (d->alignment == alignment)
        return;

    d->alignment = alignment;
    d->fitToBounds();
}

void QnGraphicsScrollArea::wheelEvent(QGraphicsSceneWheelEvent* event)
{
    Q_D(QnGraphicsScrollArea);
    event->ignore();

    if (event->orientation() != Qt::Vertical)
        return;

    if (!d->contentWidget || d->contentWidget->size().height() <= size().height())
        return;

    int dy = -event->delta() / 120 * d->lineHeight * qApp->wheelScrollLines();
    d->yOffset -= dy;
    d->fitToBounds();
    event->accept();
}

QnGraphicsScrollAreaPrivate::QnGraphicsScrollAreaPrivate(QnGraphicsScrollArea* parent):
    QObject(parent),
    q_ptr(parent)
{
    Q_Q(QnGraphicsScrollArea);
    QFontMetrics fm(q->font());
    lineHeight = fm.height();
}

void QnGraphicsScrollAreaPrivate::fitToBounds()
{
    if (!contentWidget)
        return;

    Q_Q(QnGraphicsScrollArea);

    const auto height = q->size().height();
    const auto contentHeight = contentWidget->size().height();
    const auto maxOffset = (height - contentHeight);
    const auto x = contentWidget->pos().x();

    if (contentHeight <= height)
    {
        yOffset = 0.0;
        contentWidget->setPos(x, alignment.testFlag(Qt::AlignBottom) ? maxOffset : 0.0);
    }
    else
    {
        if (yOffset > -maxOffset)
            yOffset = -maxOffset;

        if (yOffset < 0.0)
            yOffset = 0.0;

        contentWidget->setPos(x, maxOffset + yOffset);
    }
}
