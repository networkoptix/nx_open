#include "scrollable_items_widget.h"
#include "private/scrollable_items_widget_p.h"

namespace {

QSizeF contentsMarginsSize(const QGraphicsWidget* item)
{
    qreal left = 0.0, right = 0.0, top = 0.0, bottom = 0.0;
    item->getContentsMargins(&left, &top, &right, &bottom);
    return QSizeF(left + right, top + bottom);
}

} // namespace

QnScrollableItemsWidget::QnScrollableItemsWidget(QGraphicsItem* parent):
    base_type(parent),
    d_ptr(new QnScrollableItemsWidgetPrivate(this))
{
    setAcceptedMouseButtons(Qt::NoButton);
}

QnScrollableItemsWidget::~QnScrollableItemsWidget()
{
    /* This is needed for correct layout cleanup. */
    clear();
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

int QnScrollableItemsWidget::lineHeight() const
{
    Q_D(const QnScrollableItemsWidget);
    return d->lineHeight();
}

void QnScrollableItemsWidget::setLineHeight(int value)
{
    Q_D(QnScrollableItemsWidget);
    d->setLineHeight(value);
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
            auto hint = d->contentSizeHint(which, constraint) + contentsMarginsSize(this);
            if (which == Qt::MinimumSize)
                hint.setHeight(0);
            return hint;
        }

        default:
            return base_type::sizeHint(which, constraint);
    }
}
