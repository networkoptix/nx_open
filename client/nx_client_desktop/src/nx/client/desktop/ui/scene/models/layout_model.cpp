#include "layout_model.h"

#include <client_core/connection_context_aware.h>

#include <utils/common/connective.h>
#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace scene {
namespace models {

class LayoutModel::Private: public Connective<QObject>, public QnConnectionContextAware
{
    LayoutModel* const q;

public:
    Private(LayoutModel* parent);

    void setLayout(const QnLayoutResourcePtr& newLayout);

    int itemIndex(const QnUuid& itemId) const;

private:
    void at_itemAdded(const QnLayoutResourcePtr& resource, const QnLayoutItemData& item);
    void at_itemRemoved(const QnLayoutResourcePtr& resource, const QnLayoutItemData& item);
    void at_itemChanged(const QnLayoutResourcePtr& resource, const QnLayoutItemData& item);

public:
    QnUuid layoutId;
    QnLayoutResourcePtr layout;
    QList<QnUuid> itemIds;
};

LayoutModel::Private::Private(LayoutModel* parent):
    q(parent)
{
}

void LayoutModel::Private::setLayout(const QnLayoutResourcePtr& newLayout)
{
    if (layout)
    {
        layout->disconnect(this);
        itemIds.clear();
    }

    layout = newLayout;

    if (!layout)
        return;

    connect(layout, &QnLayoutResource::itemAdded, this, &Private::at_itemAdded);
    connect(layout, &QnLayoutResource::itemRemoved, this, &Private::at_itemRemoved);
    connect(layout, &QnLayoutResource::itemChanged, this, &Private::at_itemChanged);

    for (const auto& item: layout->getItems())
        itemIds.insert(std::lower_bound(itemIds.begin(), itemIds.end(), item.uuid), item.uuid);
}

int LayoutModel::Private::itemIndex(const QnUuid& itemId) const
{
    const auto it = std::lower_bound(itemIds.begin(), itemIds.end(), itemId);
    if (it != itemIds.end() && *it == itemId)
        return static_cast<int>(std::distance(itemIds.begin(), it));
    return -1;
}

void LayoutModel::Private::at_itemAdded(
    const QnLayoutResourcePtr& /*resource*/, const QnLayoutItemData& item)
{
    const auto it = std::lower_bound(itemIds.begin(), itemIds.end(), item.uuid);
    if (it != itemIds.end() && *it == item.uuid)
        return;

    const int row = static_cast<int>(std::distance(itemIds.begin(), it));
    q->beginInsertRows(QModelIndex(), row, row);
    itemIds.insert(it, item.uuid);
    q->endInsertRows();
}

void LayoutModel::Private::at_itemRemoved(
    const QnLayoutResourcePtr& /*resource*/, const QnLayoutItemData& item)
{
    const int row = itemIndex(item.uuid);
    if (row == -1)
        return;

    q->beginRemoveRows(QModelIndex(), row, row);
    itemIds.removeAt(row);
    q->endRemoveRows();
}

void LayoutModel::Private::at_itemChanged(
    const QnLayoutResourcePtr& /*resource*/, const QnLayoutItemData& item)
{
    const int row = itemIndex(item.uuid);
    if (row == -1)
        return;

    const auto& modelIndex = q->index(row);
    emit q->dataChanged(modelIndex, modelIndex);
}

//-------------------------------------------------------------------------------------------------

LayoutModel::LayoutModel(QObject* parent):
    base_type(parent),
    d(new Private(this))
{
}

LayoutModel::~LayoutModel()
{
}

QnUuid LayoutModel::layoutId() const
{
    return d->layoutId;
}

void LayoutModel::setLayoutId(const QnUuid& id)
{
    if (d->layoutId == id)
        return;

    beginResetModel();

    d->layoutId = id;
    emit layoutChanged();

    const auto& layout = d->resourcePool()->getResourceById<QnLayoutResource>(id);
    d->setLayout(layout);

    endResetModel();
}

QHash<int, QByteArray> LayoutModel::roleNames() const
{
    static QHash<int, QByteArray> kRoleNames{
        { static_cast<int>(Roles::itemId), "itemId" },
        { static_cast<int>(Roles::name), "name" },
        { static_cast<int>(Roles::geometry), "geometry" },
    };

    return kRoleNames;
}

QVariant LayoutModel::data(const QModelIndex& index, int role) const
{
    if (!hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    const auto& itemId = d->itemIds[index.row()];
    const auto& item = d->layout->getItem(itemId);

    switch (static_cast<Roles>(role))
    {
        case Roles::itemId:
            return QVariant::fromValue(itemId);

        case Roles::name:
        {
            const auto& resource = d->resourcePool()->getResourceById(item.resource.id);
            return resource ? resource->getName() : QString();
        }

        case Roles::geometry:
            return item.combinedGeometry.toRect();
    }

    return QVariant();
}

int LayoutModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;

    return d->itemIds.size();
}

} // namespace models
} // namespace scene
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
