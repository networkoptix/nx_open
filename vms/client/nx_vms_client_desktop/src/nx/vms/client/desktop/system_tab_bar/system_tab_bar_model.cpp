// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_tab_bar_model.h"

#include <client/client_globals.h>

namespace nx::vms::client::desktop {

SystemTabBarModel::SystemTabBarModel(QObject* parent):
    base_type(parent)
{
}

SystemTabBarModel::~SystemTabBarModel()
{
}

int SystemTabBarModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;

    return m_systems.size();
}

int SystemTabBarModel::columnCount(const QModelIndex& /*parent*/) const
{
    return 1;
}

QVariant SystemTabBarModel::data(const QModelIndex& index, int role) const
{
    NX_ASSERT(index.isValid() && index.model() == this && index.row() < m_systems.count());

    auto item = m_systems[index.row()];
    switch (role)
    {
        case Qt::DisplayRole:
            return QVariant::fromValue(item.systemDescription->name());

        case Qn::LogonDataRole:
            return QVariant::fromValue(item.logonData);

        case Qn::SystemDescriptionRole:
            return QVariant::fromValue(item.systemDescription);

        case Qn::WorkbenchStateRole:
            return QVariant::fromValue(item.workbenchState);

        default:
            return {};
    }
}

bool SystemTabBarModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    NX_ASSERT(index.isValid() && index.model() == this && index.row() < m_systems.count());
    auto& item = m_systems[index.row()];
    switch (role)
    {
        case Qn::WorkbenchStateRole:
            item.workbenchState = value.value<WorkbenchState>();
            return true;

        default:
            return base_type::setData(index, value, role);
    }
}

QVariant SystemTabBarModel::headerData(int, Qt::Orientation, int) const
{
    return {};
}

QModelIndex SystemTabBarModel::findSystem(const QString& systemId) const
{
    auto it = std::find_if(m_systems.cbegin(), m_systems.cend(),
        [systemId](const auto value)
        {
            return value.systemDescription->id() == systemId;
        });

    if (it == m_systems.cend())
        return {};

    return index(std::distance(m_systems.cbegin(), it));
}

void SystemTabBarModel::addSystem(const QnSystemDescriptionPtr& systemDescription,
    const LogonData& logonData)
{
    if (!systemDescription)
        return;

    SystemData value({.systemDescription = systemDescription, .logonData = std::move(logonData)});
    if (findSystem(systemDescription->id()).isValid())
        return;

    const auto count = m_systems.count();
    beginInsertRows(QModelIndex(), count, count);
    m_systems.append(value);
    endInsertRows();
    emit dataChanged(index(count), index(count));
}

void SystemTabBarModel::removeSystem(const QnSystemDescriptionPtr& systemDescription)
{
    if (systemDescription)
        removeSystem(systemDescription->id());
}

void SystemTabBarModel::removeSystem(const QString& systemId)
{
    auto modelIndex = findSystem(systemId);
    if (modelIndex.isValid())
    {
        const int i = modelIndex.row();
        beginRemoveRows(QModelIndex(), i, i);
        m_systems.removeAt(i);
        endRemoveRows();
        emit dataChanged(modelIndex, index(m_systems.count() - 1));
    }
}

void SystemTabBarModel::setSystemState(const QString& systemId, WorkbenchState state)
{
    const auto index = findSystem(systemId);
    if (NX_ASSERT(index.isValid()))
        m_systems[index.row()].workbenchState = std::move(state);
}

} // namespace nx::vms::client::desktop
