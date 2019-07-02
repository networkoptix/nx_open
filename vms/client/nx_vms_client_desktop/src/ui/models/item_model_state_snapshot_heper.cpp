#include "item_model_state_snapshot_heper.h"

#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QAbstractItemModel>

using namespace nx::vms::client::desktop;

ItemModelStateSnapshotHelper::ItemModelStateSnapshotHelper(QAbstractItemModel* model):
    m_model(model)
{
    NX_ASSERT(model);
}

QJsonDocument ItemModelStateSnapshotHelper::getItemModelStateDocument()
{
    return QJsonDocument(createItemModelObject());
}

void ItemModelStateSnapshotHelper::saveItemModelStateJson(const QString& path)
{
    QFile file(path);
    if (!file.open(QFile::WriteOnly | QFile::Truncate))
    {
        NX_ASSERT(false);
        return;
    }
    file.write(getItemModelStateDocument().toJson(QJsonDocument::Indented));
}

bool ItemModelStateSnapshotHelper::compareItemModelState(const QString& referencePath,
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

QJsonObject ItemModelStateSnapshotHelper::createItemModelObject() const
{
    QJsonObject itemModelObject;

    if ((m_model->rowCount() > 0) && (m_model->columnCount() > 0))
        itemModelObject.insert(kRowsArrayKey, createRowsArray(QModelIndex()));

    return itemModelObject;
}

QJsonObject ItemModelStateSnapshotHelper::createItemObject(const QModelIndex& index) const
{
    QJsonObject itemObject;
    if (!index.isValid())
    {
        NX_ASSERT(false);
        return itemObject;
    }

    const auto indexData = m_model->itemData(index);
    QVariantMap variantMap;
    for (auto i = indexData.cbegin(); i != indexData.cend(); ++i)
        variantMap.insert(getRoleName(i.key()), i.value());

    if (!variantMap.empty())
        itemObject.insert(kItemDataObjectKey, QJsonObject::fromVariantMap(variantMap));
    itemObject.insert(kItemFlagsValueKey, QJsonValue(static_cast<Qt::ItemFlags::Int>(index.flags())));
    if ((m_model->rowCount(index) > 0) && (m_model->columnCount(index) > 0))
        itemObject.insert(kRowsArrayKey, createRowsArray(index));

    return itemObject;
}

QJsonArray ItemModelStateSnapshotHelper::createRowsArray(const QModelIndex& parent) const
{
    QJsonArray rowsArray;
    for (int row = 0; row < m_model->rowCount(parent); ++row)
        rowsArray.append(createRowItemsArray(row, parent));
    return rowsArray;
}

QJsonArray ItemModelStateSnapshotHelper::createRowItemsArray(int row,
    const QModelIndex& parent) const
{
    QJsonArray rowItems;
    for (int column = 0; column < m_model->columnCount(parent); ++column)
        rowItems.append(createItemObject(m_model->index(row, column, parent)));
    return rowItems;
}

QString ItemModelStateSnapshotHelper::getRoleName(int role) const
{
    const auto roleNames = m_model->roleNames();
    if (roleNames.contains(role))
        return roleNames.value(role);
    return QString::number(role);
}
