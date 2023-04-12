// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "lookup_lists_model.h"

namespace nx::vms::client::desktop {

LookupListsModel::LookupListsModel(QObject* parent):
    base_type(parent)
{
}

LookupListsModel::~LookupListsModel()
{
}

QHash<int, QByteArray> LookupListsModel::roleNames() const
{
    auto roles = base_type::roleNames();
    roles[ValueRole] = "value";
    return roles;
}

int LookupListsModel::rowCount(const QModelIndex& parent) const
{
    return m_data.size();
}

QVariant LookupListsModel::data(const QModelIndex& index, int role) const
{
    const int row = index.row();
    if (row < 0 || row >= rowCount())
        return {};

    switch (role)
    {
        case Qt::DisplayRole:
            return m_data[row].name;
        case ValueRole:
            return QVariant::fromValue(m_data[row]);
    }

    return {};
}

void LookupListsModel::resetData(const nx::vms::api::LookupListDataList& data)
{
    beginResetModel();
    m_data.clear();
    m_data.reserve(data.size());
    for (const auto& list: data)
        m_data.emplace_back(list);
    endResetModel();
}

void LookupListsModel::addList(const nx::vms::api::LookupListData& list)
{
    const int count = m_data.size();
    beginInsertRows({}, count, count);
    m_data.emplace_back(list);
    endInsertRows();
}

void LookupListsModel::updateList(const nx::vms::api::LookupListData& list)
{
    auto iter = std::find_if(m_data.begin(), m_data.end(),
        [id = list.id](const auto& elem) { return elem.id == id; });

    if (iter == m_data.end())
        return;

    auto row = std::distance(m_data.begin(), iter);
    *iter = list;
    emit dataChanged(index(row), index(row));
}

void LookupListsModel::removeList(const QnUuid& id)
{
    auto iter = std::find_if(m_data.begin(), m_data.end(),
        [id](const auto& elem) { return elem.id == id; });

    if (iter == m_data.end())
        return;

    auto row = std::distance(m_data.begin(), iter);
    beginRemoveRows({}, row, row);
    m_data.erase(iter);
    endRemoveRows();
}

} // namespace nx::vms::client::desktop
