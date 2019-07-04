#pragma once

#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>

class QAbstractItemModel;

namespace nx::vms::client::desktop {

class NX_VMS_CLIENT_DESKTOP_API ItemModelStateSnapshotHelper
{
public:
    static QJsonDocument makeSnapshot(
        const QAbstractItemModel* model,
        const QModelIndex& rootIndex);

    static void saveSnapshotToFile(
        const QAbstractItemModel* model,
        const QModelIndex& rootIndex,
        const QString& path);

    static bool compareItemModelState(
        const QString& referencePath,
        const QJsonDocument& stateDocument);

private:
    static QJsonObject createItemObject(
        const QAbstractItemModel* model,
        const QModelIndex& index);

    static QJsonArray createRowsArray(
        const QAbstractItemModel* model,
        const QModelIndex& parent);

    static QJsonArray createRowItemsArray(
        const QAbstractItemModel* model,
        int row,
        const QModelIndex& parent);

    static QString getRoleName(
        const QAbstractItemModel* model,
        int role);

private:
    static constexpr auto kItemDataObjectKey = "itemData";
    static constexpr auto kItemFlagsValueKey = "itemFlags";
    static constexpr auto kRowsArrayKey = "rows";
};

} // namespace nx::vms::client::desktop
