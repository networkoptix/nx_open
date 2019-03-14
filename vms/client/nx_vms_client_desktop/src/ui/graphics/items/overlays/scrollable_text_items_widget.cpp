#include "scrollable_text_items_widget.h"

#include <nx/utils/log/assert.h>

QnScrollableTextItemsWidget::QnScrollableTextItemsWidget(QGraphicsWidget* parent):
    base_type(parent)
{
    static const QMarginsF kDefaultMargins(2.0, 28.0, 2.0, 2.0);

    setContentsMargins(kDefaultMargins.left(), kDefaultMargins.top(),
        kDefaultMargins.right(), kDefaultMargins.bottom());

    setAlignment(Qt::AlignRight | Qt::AlignBottom);
    setFlag(QGraphicsItem::ItemClipsChildrenToShape);
}

QnScrollableTextItemsWidget::~QnScrollableTextItemsWidget()
{
}

QnUuid QnScrollableTextItemsWidget::addItem(const QString& text,
    const QnHtmlTextItemOptions& options, const QnUuid& externalId)
{
    return insertItem(count(), text, options, externalId);
}

QnUuid QnScrollableTextItemsWidget::insertItem(int index, const QString& text,
    const QnHtmlTextItemOptions& options, const QnUuid& externalId)
{
    if (!externalId.isNull() && item(externalId))
        return QnUuid();

    const auto item = new QnHtmlTextItem(text, options, this);
    const auto id = base_type::insertItem(index, item, externalId);

    NX_ASSERT(!id.isNull());
    return id;
}
