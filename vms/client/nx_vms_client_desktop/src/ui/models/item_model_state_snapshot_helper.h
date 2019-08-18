#pragma once

#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QModelIndex>
#include <QtCore/QSet>

#include <client/client_globals.h>

class QAbstractItemModel;

namespace nx::vms::client::desktop {

class ItemModelStateSnapshotHelper
{
public:
    struct SnapshotParams
    {
        QModelIndex parentIndex = QModelIndex();
        QSet<int> roles = {Qt::DisplayRole, Qn::ResourceIconKeyRole};
        std::optional<int> startRow; //< Applied only for children of item at parentIndex
        std::optional<int> rowCount; //< Applied only for children of item at parentIndex
        std::optional<int> depth;
    };

    static QJsonDocument makeSnapshot(
        const QAbstractItemModel* model,
        const SnapshotParams& params);

    static void saveSnapshotToFile(
        const QAbstractItemModel* model,
        const SnapshotParams& params,
        const QString& path);

private:
    static QJsonObject createItemObject(
        const QAbstractItemModel* model,
        const QModelIndex& index,
        const SnapshotParams& params);

    static QJsonArray createChildrenArray(
        const QAbstractItemModel* model,
        const SnapshotParams& params);

    static QString getRoleName(
        const QAbstractItemModel* model,
        int role);

    static QVariant prepareRoleData(
        const QVariant& rawData,
        int role);

private:
    static constexpr auto kItemDataObjectKey = "data";
    static constexpr auto kChildrenArrayKey = "children";
};

} // namespace nx::vms::client::desktop
