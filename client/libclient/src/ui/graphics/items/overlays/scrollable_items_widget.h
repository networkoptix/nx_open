#pragma once

#include <QtWidgets/QGraphicsWidget>

class QnScrollableItemsWidgetPrivate;

/**
  *  Container class that displays graphics items on a scrollable area.
  */
class QnScrollableItemsWidget: public QGraphicsWidget
{
    Q_OBJECT
    using base_type = QGraphicsWidget;

public:
    QnScrollableItemsWidget(QGraphicsItem* parent = nullptr);
    virtual ~QnScrollableItemsWidget();

    /** Alignment of items inside scrollable area. */
    Qt::Alignment alignment() const;
    void setAlignment(Qt::Alignment alignment);

    /** Vertical spacing of items inside scrollable area. */
    qreal spacing() const;
    void setSpacing(qreal value);

    /** Add item to scrollable area at the end. Ownership of the item is taken.
      * If externalId is not null it's used as unique item id, otherwise it is autogenerated.
      * If item is successfully added, its unique id is returned.
      * If another item with specified externalId already exists,
      *    new item isn't added and null uuid is returned. */
    QnUuid addItem(QGraphicsWidget* item, const QnUuid& externalId = QnUuid());

    /** Insert item to scrollable area at specified position. Ownership of the item is taken.
      * If externalId is not null it's used as unique item id, otherwise it is autogenerated.
      * If item is successfully added, its unique id is returned.
      * If another item with specified externalId already exists,
      *    new item isn't added and null uuid is returned. */
    QnUuid insertItem(int index, QGraphicsWidget* item, const QnUuid& externalId = QnUuid());

    /** Remove item from scrollable area and return it. Item ownership is passed to the caller.
      * If no item with specified id exists, nullptr is returned. */
    QGraphicsWidget* takeItem(const QnUuid& id);

    /** Remove item from scrollable area and delete the item.
      * Returns true if an item is found and deleted, false otherwise. */
    bool deleteItem(const QnUuid& id);

    /** Remove all items from scrollable area and delete them. */
    void clear();

    /** Count of items in scrollable area. */
    int count() const;

    /** Item by index in scrollable area. Returns nullptr if index is out of range. */
    QGraphicsWidget* item(int index) const;

    /** Item by unique id. Returns nullptr if an item with specified id is not found. */
    QGraphicsWidget* item(const QnUuid& id) const;

protected:
    virtual QSizeF sizeHint(Qt::SizeHint which,
        const QSizeF& constraint = QSizeF()) const override;

private:
    Q_DECLARE_PRIVATE(QnScrollableItemsWidget);
    QnScrollableItemsWidgetPrivate* const d_ptr;
};
