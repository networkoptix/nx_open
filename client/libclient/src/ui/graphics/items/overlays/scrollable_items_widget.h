#pragma once

#include <QtWidgets/QGraphicsWidget>

class QnScrollableItemsWidgetPrivate;
class QnScrollableItemsWidget: public QGraphicsWidget
{
    Q_OBJECT
    using base_type = QGraphicsWidget;

public:
    QnScrollableItemsWidget(Qt::Alignment alignment, QGraphicsItem* parent = nullptr);

    /** Add item to scrollable area. Will take ownership of the item. */
    QnUuid addItem(QGraphicsWidget* item, const QnUuid& externalId = QnUuid());
    QnUuid insertItem(int index, QGraphicsWidget* item, const QnUuid& externalId = QnUuid());

    QGraphicsWidget* takeItem(const QnUuid& id);
    bool deleteItem(const QnUuid& id);
    void clear();

    int count() const;

    QGraphicsWidget* item(int index) const;
    QGraphicsWidget* item(const QnUuid& id) const;

    qreal spacing() const;
    void setSpacing(qreal value);

protected:
    virtual QSizeF sizeHint(Qt::SizeHint which,
        const QSizeF& constraint = QSizeF()) const override;

private:
    Q_DECLARE_PRIVATE(QnScrollableItemsWidget);
    QnScrollableItemsWidgetPrivate* const d_ptr;
};
