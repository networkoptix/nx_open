#include "scrollable_items_widget.h"
#include "private/scrollable_items_widget_p.h"

QnScrollableItemsWidget::QnScrollableItemsWidget(QGraphicsItem* parent):
    base_type(parent),
    d_ptr(new QnScrollableItemsWidgetPrivate(this))
{
}

Qt::Alignment QnScrollableItemsWidget::alignment() const
{
    Q_D(const QnScrollableItemsWidget);
    return d->alignment();
}

void QnScrollableItemsWidget::setAlignment(Qt::Alignment alignment)
{
    Q_D(QnScrollableItemsWidget);
    d->setAlignment(alignment);
}

QnUuid QnScrollableItemsWidget::addItem(QGraphicsWidget* item, const QnUuid& externalId)
{
    return insertItem(count(), item, externalId);
}

QnUuid QnScrollableItemsWidget::insertItem(int index, QGraphicsWidget* item, const QnUuid& externalId)
{
    Q_D(QnScrollableItemsWidget);
    return d->insertItem(index, item, externalId);
}

QGraphicsWidget* QnScrollableItemsWidget::takeItem(const QnUuid& id)
{
    Q_D(QnScrollableItemsWidget);
    return d->takeItem(id);
}

bool QnScrollableItemsWidget::deleteItem(const QnUuid& id)
{
    Q_D(QnScrollableItemsWidget);
    auto item = d->takeItem(id);
    if (!item)
        return false;

    delete item;
    return true;
}

void QnScrollableItemsWidget::clear()
{
    Q_D(QnScrollableItemsWidget);
    d->clear();
}

int QnScrollableItemsWidget::count() const
{
    Q_D(const QnScrollableItemsWidget);
    return d->count();
}

QGraphicsWidget* QnScrollableItemsWidget::item(int index) const
{
    Q_D(const QnScrollableItemsWidget);
    return d->item(index);
}

QGraphicsWidget* QnScrollableItemsWidget::item(const QnUuid& id) const
{
    Q_D(const QnScrollableItemsWidget);
    return d->item(id);
}

qreal QnScrollableItemsWidget::spacing() const
{
    Q_D(const QnScrollableItemsWidget);
    return d->spacing();
}

void QnScrollableItemsWidget::setSpacing(qreal value)
{
    Q_D(QnScrollableItemsWidget);
    d->setSpacing(value);
}

QSizeF QnScrollableItemsWidget::sizeHint(Qt::SizeHint which, const QSizeF& constraint) const
{
    switch (which)
    {
        case Qt::MinimumSize:
        case Qt::PreferredSize:
        {
            Q_D(const QnScrollableItemsWidget);
            auto hint = d->contentSizeHint(which, constraint);
            if (which == Qt::MinimumSize)
                hint.setHeight(0);
            return hint;
        }

        default:
            return base_type::sizeHint(which, constraint);
    }
}
