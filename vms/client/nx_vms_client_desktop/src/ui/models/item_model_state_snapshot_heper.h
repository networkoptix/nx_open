#pragma once

#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>

class QAbstractItemModel;

namespace nx::vms::client::desktop {

class ItemModelStateSnapshotHelper
{
public:
    ItemModelStateSnapshotHelper(QAbstractItemModel* model);
    QJsonDocument getItemModelStateDocument();
    void saveItemModelStateJson(const QString& path);
    bool compareItemModelState(const QString& referencePath, const QJsonDocument& stateDocument);

private:
    QJsonObject createItemModelObject() const;
    QJsonObject createItemObject(const QModelIndex& index) const;
    QJsonArray createRowsArray(const QModelIndex& parent) const;
    QJsonArray createRowItemsArray(int row, const QModelIndex& parent) const;
    QString getRoleName(int role) const;

private:
    static constexpr auto kItemDataObjectKey = "itemData";
    static constexpr auto kItemFlagsValueKey = "itemFlags";
    static constexpr auto kRowsArrayKey = "rows";

    QAbstractItemModel* m_model = nullptr;
};

} // namespace nx::vms::client::desktop
