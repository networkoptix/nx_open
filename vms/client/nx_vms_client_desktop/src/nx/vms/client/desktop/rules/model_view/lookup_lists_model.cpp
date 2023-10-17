// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "lookup_lists_model.h"

#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/lookup_lists/lookup_list_manager.h>

namespace nx::vms::client::desktop::rules {

LookupListsModel::LookupListsModel(SystemContext* context, QObject* parent):
    QAbstractListModel{parent},
    SystemContextAware{context}
{
    const auto lookupListManager = systemContext()->lookupListManager();

    connect(
        lookupListManager,
        &common::LookupListManager::addedOrUpdated,
        this,
        [this](QnUuid id)
        {
            const auto affectedList = systemContext()->lookupListManager()->lookupList(id);

            if (m_objectTypeId != affectedList.objectTypeId)
                return;

            auto lookupListIt = std::find_if(
                m_lookupLists.begin(),
                m_lookupLists.end(),
                [&affectedList](const std::pair<QnUuid, QString>& listEntry)
                {
                    return listEntry.first == affectedList.id;
                });

            if (lookupListIt == m_lookupLists.end())
            {
                beginInsertRows({}, m_lookupLists.size(), m_lookupLists.size());
                m_lookupLists.emplace_back(affectedList.id, affectedList.name);
                endInsertRows();
                return;
            }

            if (lookupListIt->second != affectedList.name)
            {
                lookupListIt->second = affectedList.name;
                const auto index = createIndex(std::distance(m_lookupLists.begin(), lookupListIt), 0);
                emit dataChanged(index, index, {Qt::DisplayRole});
            }
        });

    connect(
        lookupListManager,
        &common::LookupListManager::removed,
        this,
        [this](QnUuid id)
        {
            const auto lookupListIt = std::find_if(
                m_lookupLists.cbegin(),
                m_lookupLists.cend(),
                [id](const std::pair<QnUuid, QString>& listEntry)
                {
                    return listEntry.first == id;
                });

            if (lookupListIt == m_lookupLists.cend())
                return;

            const auto distance = std::distance(m_lookupLists.cbegin(), lookupListIt);
            beginRemoveRows({}, distance, distance);
            m_lookupLists.erase(lookupListIt);
            endRemoveRows();
        });
}

std::optional<QString> LookupListsModel::objectTypeId() const
{
    return m_objectTypeId;
}

void LookupListsModel::setObjectTypeId(const QString& type)
{
    if (m_objectTypeId == type)
        return;

    m_objectTypeId = type;

    beginResetModel();

    m_lookupLists.clear();
    for (const auto& list: systemContext()->lookupListManager()->lookupLists())
    {
        if (m_objectTypeId == list.objectTypeId)
            m_lookupLists.emplace_back(list.id, list.name);
    }

    endResetModel();
}

int LookupListsModel::rowCount(const QModelIndex& /*parent*/) const
{
    return m_lookupLists.size();
}

QVariant LookupListsModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_lookupLists.size())
        return {};

    if (role == Qt::DisplayRole)
        return m_lookupLists[index.row()].second;

    if (role == LookupListIdRole)
        return QVariant::fromValue(m_lookupLists[index.row()].first);

    return {};
}

} // namespace nx::vms::client::desktop::rules
