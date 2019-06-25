#include "item_selection_helper.h"

#include <QtQml/QtQml>

namespace nx::vms::client::desktop {

QItemSelection ItemSelectionHelper::createSelection(
    const QModelIndex& topLeft, const QModelIndex& bottomRight) const
{
    return QItemSelection(topLeft, bottomRight);
}

void ItemSelectionHelper::registerQmlType()
{
    qmlRegisterType<ItemSelectionHelper>("nx.vms.client.desktop", 1, 0, "ItemSelectionHelper");
};

} // namespace nx::vms::client::desktop
