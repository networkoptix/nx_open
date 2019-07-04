#include "item_model_state_snapshot_helper.h"

#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QAbstractItemModel>

using namespace nx::vms::client::desktop;

QJsonDocument ItemModelStateSnapshotHelper::makeSnapshot(
    const QAbstractItemModel* model,
    const QModelIndex& rootIndex)
{
   return QJsonDocument(createRowsArray(model, rootIndex));
}

void ItemModelStateSnapshotHelper::saveSnapshotToFile(
    const QAbstractItemModel* model,
    const QModelIndex& rootIndex,
    const QString& path)
{
    QFile file(path);
    if (!file.open(QFile::WriteOnly | QFile::Truncate))
    {
        NX_ASSERT(false);
        return;
    }
    file.write(makeSnapshot(model, rootIndex).toJson(QJsonDocument::Indented));
}

bool ItemModelStateSnapshotHelper::compareItemModelState(
    const QString& referencePath,
    const QJsonDocument& stateDocument)
{
    QFile file(referencePath);
    if (!file.open(QFile::ReadOnly))
    {
        NX_ASSERT(false);
        return false;
    }
    QJsonParseError parseError = {};
    const auto referenceDocument = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError)
    {
        NX_ASSERT(false);
        return false;
    }
    return referenceDocument == stateDocument;
}

QJsonObject ItemModelStateSnapshotHelper::createItemObject(
    const QAbstractItemModel* model,
    const QModelIndex& index)
{
    QJsonObject itemObject;
    if (!index.isValid())
    {
        NX_ASSERT(false);
        return itemObject;
    }

    const auto indexData = model->itemData(index);
    QVariantMap variantMap;
    for (auto i = indexData.cbegin(); i != indexData.cend(); ++i)
        variantMap.insert(getRoleName(model, i.key()), i.value());

    if (!variantMap.empty())
        itemObject.insert(kItemDataObjectKey, QJsonObject::fromVariantMap(variantMap));
    itemObject.insert(kItemFlagsValueKey,
        QJsonValue(static_cast<Qt::ItemFlags::Int>(index.flags())));
    if ((model->rowCount(index) > 0) && (model->columnCount(index) > 0))
        itemObject.insert(kRowsArrayKey, createRowsArray(model, index));

    return itemObject;
}

QJsonArray ItemModelStateSnapshotHelper::createRowsArray(
    const QAbstractItemModel* model,
    const QModelIndex& parent)
{
    QJsonArray rowsArray;
    for (int row = 0; row < model->rowCount(parent); ++row)
        rowsArray.append(createRowItemsArray(model, row, parent));
    return rowsArray;
}

QJsonArray ItemModelStateSnapshotHelper::createRowItemsArray(
    const QAbstractItemModel* model,
    int row,
    const QModelIndex& parent)
{
    QJsonArray rowItems;
    for (int column = 0; column < model->columnCount(parent); ++column)
        rowItems.append(createItemObject(model, model->index(row, column, parent)));
    return rowItems;
}

QString ItemModelStateSnapshotHelper::getRoleName(const QAbstractItemModel* model, int role)
{
    const auto roleNames = model->roleNames();
    if (roleNames.contains(role))
        return roleNames.value(role);
    return QString::number(role);
}
