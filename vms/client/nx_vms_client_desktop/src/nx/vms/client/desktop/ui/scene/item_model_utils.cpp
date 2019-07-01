#include "item_model_utils.h"

#include <QtCore/QAbstractItemModel>
#include <QtCore/QScopedPointer>
#include <QtQml/QtQml>

#include <nx/utils/log/assert.h>

namespace nx::vms::client::desktop {

QItemSelection ItemModelUtils::createSelection(
    const QModelIndex& topLeft, const QModelIndex& bottomRight)
{
    return QItemSelection(topLeft, bottomRight);
}

QMimeData* ItemModelUtils::mimeData(const QModelIndexList& indices)
{
    const QAbstractItemModel* model = nullptr;
    QModelIndexList filtered;

    for (const auto& index: indices)
    {
        if (!index.isValid())
            continue;

        if (!model)
            model = index.model();
        else if (!NX_ASSERT(model == index.model()))
            continue;

        if (index.flags().testFlag(Qt::ItemIsDragEnabled))
            filtered.push_back(index);
    }

    if (filtered.empty() || !NX_ASSERT(model))
        return {};

    return model->mimeData(filtered);
}

void ItemModelUtils::registerQmlType()
{
    qmlRegisterSingletonType<ItemModelUtils>("nx.vms.client.desktop", 1, 0, "ItemModelUtils",
        [](QQmlEngine*, QJSEngine*) -> QObject* { return new ItemModelUtils(); });

    qRegisterMetaType<QMimeData*>();
};

} // namespace nx::vms::client::desktop
