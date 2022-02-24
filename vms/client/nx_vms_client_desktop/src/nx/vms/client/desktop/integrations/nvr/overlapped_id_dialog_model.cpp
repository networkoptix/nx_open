// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "overlapped_id_dialog_model.h"

namespace nx::vms::client::desktop::integrations {

OverlappedIdDialogModel::OverlappedIdDialogModel(QObject* parent): base_type(parent)
{
}

void OverlappedIdDialogModel::setIdList(const std::vector<int>& idList)
{
    beginResetModel();
    m_idList = idList;
    endResetModel();
}

QHash<int, QByteArray> OverlappedIdDialogModel::roleNames() const
{
    auto roleNames = base_type::roleNames();
    roleNames[IdRole] = "id";
    return roleNames;
}

QVariant OverlappedIdDialogModel::data(const QModelIndex& index, int role) const
{
    if (index.row() < 0 || rowCount() <= index.row()
        || index.column() < 0 || columnCount() <= index.column()
        || !index.isValid())
    {
        return QVariant();
    }

    switch (role)
    {
        case Qt::DisplayRole:
        {
            if (index.row() == 0)
                return tr("Latest");
            return m_idList[index.row() - 1];
        }
        case IdRole:
        {
            if (index.row() == 0)
                return -1;
            return m_idList[index.row() - 1];
        }
        default:
            return QVariant();
    }
}

QModelIndex OverlappedIdDialogModel::index(int row, int column, const QModelIndex& parent) const
{
    if (row < 0 || rowCount(parent) <= row || column < 0 || columnCount(parent) <= column)
        return QModelIndex();

    return createIndex(row, column);
}

QModelIndex OverlappedIdDialogModel::parent(const QModelIndex& index) const
{
    return QModelIndex();
}

int OverlappedIdDialogModel::rowCount(const QModelIndex& parent) const
{
    return m_idList.size() + 1;
}

int OverlappedIdDialogModel::columnCount(const QModelIndex& parent) const
{
    return 1;
}

} // namespace nx::vms::client::desktop::integrations
