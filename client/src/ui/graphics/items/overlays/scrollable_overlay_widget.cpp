#include "scrollable_overlay_widget.h"

#include <ui/graphics/items/overlays/private/scrollable_overlay_widget_p.h>

QnScrollableOverlayWidget::QnScrollableOverlayWidget(Qt::Alignment alignment, QGraphicsWidget *parent /*= nullptr*/ )
    : base_type(parent)
    , d_ptr(new QnScrollableOverlayWidgetPrivate(alignment, this))
{
    Q_D(QnScrollableOverlayWidget);

    setFlag(QGraphicsItem::ItemClipsChildrenToShape);
    setAcceptedMouseButtons(0);
    setLayout(d->m_mainLayout);
}

QnScrollableOverlayWidget::~QnScrollableOverlayWidget() {
}

QnUuid QnScrollableOverlayWidget::addItem( QGraphicsWidget *item, const QnUuid &externalId)
{
    Q_D(QnScrollableOverlayWidget);
    auto result = d->addItem(item, externalId);
    emit contentSizeChanged();
    return result;
}

QnUuid QnScrollableOverlayWidget::insertItem(int index, QGraphicsWidget *item, const QnUuid &externalId /*= QnUuid()*/)
{
    Q_D(QnScrollableOverlayWidget);
    auto result = d->insertItem(index, item, externalId);
    emit contentSizeChanged();
    return result;
}

void QnScrollableOverlayWidget::removeItem( const QnUuid &id )
{
    Q_D(QnScrollableOverlayWidget);
    d->removeItem(id);
    emit contentSizeChanged();
}

void QnScrollableOverlayWidget::clear()
{
    Q_D(QnScrollableOverlayWidget);
    d->clear();
    emit contentSizeChanged();
}

int QnScrollableOverlayWidget::overlayWidth() const
{
    Q_D(const QnScrollableOverlayWidget);
    return d->overlayWidth();
}

void QnScrollableOverlayWidget::setOverlayWidth( int width )
{
    Q_D(QnScrollableOverlayWidget);
    d->setOverlayWidth(width);
}

QSizeF QnScrollableOverlayWidget::sizeHint( Qt::SizeHint which, const QSizeF &constraint /*= QSizeF()*/ ) const
{
    if (which != Qt::MinimumSize)
        return base_type::sizeHint(which, constraint);

    Q_D(const QnScrollableOverlayWidget);
    return d->minimumSize();
}

QSizeF QnScrollableOverlayWidget::maxFillCoeff() const
{
    Q_D(const QnScrollableOverlayWidget);
    return d->maxFillCoeff();
}

void QnScrollableOverlayWidget::setMaxFillCoeff( const QSizeF &coeff )
{
    Q_D(QnScrollableOverlayWidget);
    d->setMaxFillCoeff(coeff);
}

QSizeF QnScrollableOverlayWidget::contentSize() const
{
    Q_D(const QnScrollableOverlayWidget);
    return d->contentSize();
}
