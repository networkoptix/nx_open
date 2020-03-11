#pragma once

#include <QtQml/QQmlListProperty>

#include "item.h"

namespace nx::vms::server::interactive_settings::components {

/** Base class for all grouping items: items which contain other items. */
class Group: public Item
{
    Q_OBJECT
    Q_PROPERTY(
        // Qt MOC requires full namespace to make the property functional.
        QQmlListProperty<nx::vms::server::interactive_settings::components::Item> items
            READ items)
    Q_PROPERTY(QVariantList filledCheckItems READ filledCheckItems WRITE setFilledCheckItems
            NOTIFY filledCheckItemsChanged)
    Q_CLASSINFO("DefaultProperty", "items")

    using base_type = Item;

public:
    using Item::Item;

    QQmlListProperty<Item> items();
    const QList<Item*> itemsList() const;

    QVariantList filledCheckItems() const { return m_filledCheckItems; }
    void setFilledCheckItems(const QVariantList& items);

    virtual QJsonObject serializeModel() const override;

signals:
    void filledCheckItemsChanged();

private:
    QList<Item*> m_items;
    QVariantList m_filledCheckItems;
};

} // namespace nx::vms::server::interactive_settings::components
