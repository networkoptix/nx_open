#include "item_model_state_snapshot_helper.h"

#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QAbstractItemModel>

#include <ui/style/resource_icon_cache.h>
#include <nx/utils/log/assert.h>

using namespace nx::vms::client::desktop;

QJsonDocument ItemModelStateSnapshotHelper::makeSnapshot(
    const QAbstractItemModel* model,
    const SnapshotParams& params)
{
    return QJsonDocument(createChildrenArray(model, params));
}

void ItemModelStateSnapshotHelper::saveSnapshotToFile(
    const QAbstractItemModel* model,
    const SnapshotParams& params,
    const QString& path)
{
    QFile file(path);
    if (!file.open(QFile::WriteOnly | QFile::Truncate))
    {
        NX_ASSERT(false);
        return;
    }
    file.write(makeSnapshot(model, params).toJson(QJsonDocument::Indented));
}

QJsonObject ItemModelStateSnapshotHelper::createItemObject(
    const QAbstractItemModel* model,
    const QModelIndex& index,
    const SnapshotParams& params)
{
    QJsonObject itemObject;
    if (!index.isValid())
    {
        NX_ASSERT(false);
        return itemObject;
    }

    QVariantMap variantMap;
    for (auto role: params.roles)
    {
        const auto data = model->data(index, role);
        if (!data.isNull())
            variantMap.insert(getRoleName(model, role), prepareRoleData(data, role));
    }
    if (!variantMap.empty())
        itemObject.insert(kItemDataObjectKey, QJsonObject::fromVariantMap(variantMap));

    if (!params.depth || *params.depth > 0)
    {
        if ((model->rowCount(index) > 0) && (model->columnCount(index) > 0))
        {
            SnapshotParams childrenParams;
            childrenParams.parentIndex = index;
            childrenParams.roles = params.roles;
            if (params.depth)
                childrenParams.depth = *params.depth - 1;
            itemObject.insert(kChildrenArrayKey, createChildrenArray(model, childrenParams));
        }
    }

    return itemObject;
}

QJsonArray ItemModelStateSnapshotHelper::createChildrenArray(
    const QAbstractItemModel* model,
    const SnapshotParams& params)
{
    const int startRow = params.startRow ? *params.startRow : 0;
    const int rowCount =
        params.rowCount ? *params.rowCount : model->rowCount(params.parentIndex) - startRow;
    NX_ASSERT(rowCount >= 0);

    QJsonArray rowsArray;
    for (int row = startRow; row < startRow + rowCount; ++row)
    {
        rowsArray.append(createItemObject(
            model, model->index(row, 0, params.parentIndex), params));
    }
    return rowsArray;
}

QString ItemModelStateSnapshotHelper::getRoleName(const QAbstractItemModel* model, int role)
{
    const auto roleNames = model->roleNames();
    if (roleNames.contains(role))
        return roleNames.value(role);
    NX_ASSERT(false);
    return QString::number(role);
}

QVariant ItemModelStateSnapshotHelper::prepareRoleData(const QVariant& rawData, int role)
{
    if (role == Qn::ResourceIconKeyRole)
    {
        const auto key = static_cast<QnResourceIconCache::Key>(rawData.toInt());
        return QnResourceIconCache::keyToString(key);
    }
    return rawData;
}
