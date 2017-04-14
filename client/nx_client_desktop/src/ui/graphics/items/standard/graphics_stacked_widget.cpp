#include "graphics_stacked_widget.h"
#include "graphics_stacked_widget_p.h"

QnGraphicsStackedWidget::QnGraphicsStackedWidget(QGraphicsItem* parent, Qt::WindowFlags flags):
    base_type(parent, flags),
    d_ptr(new QnGraphicsStackedWidgetPrivate(this))
{
    setAcceptedMouseButtons(Qt::NoButton);
}

QnGraphicsStackedWidget::~QnGraphicsStackedWidget()
{
    /* d_ptr must be destructed before base_type destructor is called.
     * For that reason QScopedPointer is used. */
}

int QnGraphicsStackedWidget::addWidget(QGraphicsWidget* widget)
{
    return insertWidget(count(), widget);
}

int QnGraphicsStackedWidget::insertWidget(int index, QGraphicsWidget* widget)
{
    Q_D(QnGraphicsStackedWidget);
    return d->insertWidget(index, widget);
}

QGraphicsWidget* QnGraphicsStackedWidget::removeWidget(int index)
{
    Q_D(QnGraphicsStackedWidget);
    return d->removeWidget(index);
}

int QnGraphicsStackedWidget::removeWidget(QGraphicsWidget* widget)
{
    Q_D(QnGraphicsStackedWidget);
    return d->removeWidget(widget);
}

int QnGraphicsStackedWidget::count() const
{
    Q_D(const QnGraphicsStackedWidget);
    return d->count();
}

int QnGraphicsStackedWidget::indexOf(QGraphicsWidget* widget) const
{
    Q_D(const QnGraphicsStackedWidget);
    return d->indexOf(widget);
}

QGraphicsWidget* QnGraphicsStackedWidget::widgetAt(int index) const
{
    Q_D(const QnGraphicsStackedWidget);
    return d->widgetAt(index);
}

Qt::Alignment QnGraphicsStackedWidget::alignment(QGraphicsWidget* widget) const
{
    Q_D(const QnGraphicsStackedWidget);
    return d->alignment(widget);
}

Qt::Alignment QnGraphicsStackedWidget::alignment(int index) const
{
    return alignment(widgetAt(index));
}

void QnGraphicsStackedWidget::setAlignment(QGraphicsWidget* widget, Qt::Alignment alignment)
{
    Q_D(QnGraphicsStackedWidget);
    d->setAlignment(widget, alignment);
}

void QnGraphicsStackedWidget::setAlignment(int index, Qt::Alignment alignment)
{
    setAlignment(widgetAt(index), alignment);
}

int QnGraphicsStackedWidget::currentIndex() const
{
    Q_D(const QnGraphicsStackedWidget);
    return d->currentIndex();
}

QGraphicsWidget* QnGraphicsStackedWidget::setCurrentIndex(int index)
{
    Q_D(QnGraphicsStackedWidget);
    return d->setCurrentIndex(index);
}

QGraphicsWidget* QnGraphicsStackedWidget::currentWidget() const
{
    Q_D(const QnGraphicsStackedWidget);
    return d->currentWidget();
}

int QnGraphicsStackedWidget::setCurrentWidget(QGraphicsWidget* widget)
{
    Q_D(QnGraphicsStackedWidget);
    return d->setCurrentWidget(widget);
}

QStackedLayout::StackingMode QnGraphicsStackedWidget::stackingMode() const
{
    Q_D(const QnGraphicsStackedWidget);
    return d->stackingMode();
}

void QnGraphicsStackedWidget::setStackingMode(QStackedLayout::StackingMode mode)
{
    Q_D(QnGraphicsStackedWidget);
    return d->setStackingMode(mode);
}

QSizeF QnGraphicsStackedWidget::sizeHint(Qt::SizeHint which, const QSizeF& constraint) const
{
    Q_D(const QnGraphicsStackedWidget);
    return d->sizeHint(which, constraint);
}
