// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_model.h"

#include <QtQml/QtQml>

#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/common/utils/common_module_aware.h>

#include "layout_item_adaptor.h"

namespace nx::vms::client::desktop {

using ItemAdaptorPtr = QSharedPointer<LayoutItemAdaptor>;

class LayoutModel::Private:
    public QObject,
    public nx::vms::client::core::CommonModuleAware
{
    LayoutModel* const q;

public:
    Private(LayoutModel* parent);

    void setLayout(const QnLayoutResourcePtr& newLayout);

    int itemIndex(const QnUuid& itemId) const;

    void updateGridBoundingRect();

private:
    void at_itemAdded(const QnLayoutResourcePtr& resource, const QnLayoutItemData& item);
    void at_itemRemoved(const QnLayoutResourcePtr& resource, const QnLayoutItemData& item);
    void at_itemChanged(const QnLayoutResourcePtr& resource, const QnLayoutItemData& item);

    void setGridBoundingRect(const QRect& rect);

public:
    QnUuid layoutId;
    QnLayoutResourcePtr layout;
    QList<QnUuid> itemIds;
    QHash<QnUuid, ItemAdaptorPtr> adaptorById;
    QRect gridBoundingRect;
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
    {
        updateGridBoundingRect();
        return;
    }

    connect(layout.get(), &QnLayoutResource::itemAdded, this, &Private::at_itemAdded);
    connect(layout.get(), &QnLayoutResource::itemRemoved, this, &Private::at_itemRemoved);
    connect(layout.get(), &QnLayoutResource::itemChanged, this, &Private::at_itemChanged);

    for (const auto& item: layout->getItems())
        itemIds.insert(std::lower_bound(itemIds.begin(), itemIds.end(), item.uuid), item.uuid);

    updateGridBoundingRect();
}

int LayoutModel::Private::itemIndex(const QnUuid& itemId) const
{
    const auto it = std::lower_bound(itemIds.begin(), itemIds.end(), itemId);
    if (it != itemIds.end() && *it == itemId)
        return static_cast<int>(std::distance(itemIds.begin(), it));
    return -1;
}

void LayoutModel::Private::updateGridBoundingRect()
{
    if (!layout)
    {
        setGridBoundingRect(QRect());
        return;
    }

    const auto& items = layout->getItems();

    if (items.isEmpty())
    {
        setGridBoundingRect(QRect());
        return;
    }

    auto rect = items.begin()->combinedGeometry.toRect();
    for (auto it = ++items.begin(); it != items.end(); ++it)
        rect = rect.united(it->combinedGeometry.toRect());

    setGridBoundingRect(rect);
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

    updateGridBoundingRect();
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

    adaptorById.remove(item.uuid);

    updateGridBoundingRect();
}

void LayoutModel::Private::at_itemChanged(
    const QnLayoutResourcePtr& /*resource*/, const QnLayoutItemData& item)
{
    const int row = itemIndex(item.uuid);
    if (row == -1)
        return;

    const auto& modelIndex = q->index(row);
    emit q->dataChanged(modelIndex, modelIndex);

    updateGridBoundingRect();
}

void LayoutModel::Private::setGridBoundingRect(const QRect& rect)
{
    if (gridBoundingRect == rect)
        return;

    gridBoundingRect = rect;
    emit q->gridBoundingRectChanged();
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

QRect LayoutModel::gridBoundingRect() const
{
    return d->gridBoundingRect;
}

QHash<int, QByteArray> LayoutModel::roleNames() const
{
    static QHash<int, QByteArray> kRoleNames{
        { static_cast<int>(Roles::itemId), "itemId" },
        { static_cast<int>(Roles::itemData), "itemData" },
    };

    return kRoleNames;
}

QVariant LayoutModel::data(const QModelIndex& index, int role) const
{
    if (!hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    const auto& itemId = d->itemIds[index.row()];

    switch (static_cast<Roles>(role))
    {
        case Roles::itemId:
            return QVariant::fromValue(itemId);

        case Roles::itemData:
        {
            if (!d->layout)
                return QVariant();

            auto adaptor = d->adaptorById.value(itemId);
            if (!adaptor)
            {
                adaptor = ItemAdaptorPtr(new LayoutItemAdaptor(d->layout, itemId));
                d->adaptorById.insert(itemId, adaptor);
            }
            return QVariant::fromValue(adaptor.data());
        }
    }

    return QVariant();
}

int LayoutModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;

    return d->itemIds.size();
}

void LayoutModel::registerQmlType()
{
    qmlRegisterType<LayoutModel>("nx.client.desktop", 1, 0, "LayoutModel");
    qmlRegisterUncreatableType<LayoutItemAdaptor>("nx.client.desktop", 1, 0, "LayoutItemAdaptor",
        lit("Cannot create instance of LayoutItemAdaptor."));
}

} // namespace nx::vms::client::desktop
