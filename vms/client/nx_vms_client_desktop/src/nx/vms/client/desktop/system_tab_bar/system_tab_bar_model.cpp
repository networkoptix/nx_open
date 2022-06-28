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

    auto value = m_systems[index.row()];
    switch (role)
    {
        case Qt::DisplayRole:
            return QVariant::fromValue(value.systemDescription->name());

        case Qn::LogonDataRole:
            return QVariant::fromValue(value.logonData);

        case Qn::SystemDescriptionRole:
            return QVariant::fromValue(value.systemDescription);

        default:
            return {};
    }
}

QVariant SystemTabBarModel::headerData(int, Qt::Orientation, int) const
{
    return {};
}

void SystemTabBarModel::addSystem(const QnSystemDescriptionPtr& systemDescription,
    const LogonData& logonData)
{
    SystemData value({.systemDescription = systemDescription, .logonData = std::move(logonData)});
    for (auto it = m_systems.cbegin(); it != m_systems.cend(); ++it)
    {
        if (it->systemDescription->id() == systemDescription->id())
            return;
    }
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
    auto it = std::find_if(m_systems.cbegin(), m_systems.cend(),
        [systemId](const auto value)
        {
            return value.systemDescription->id() == systemId;
        });

    if (it != m_systems.cend())
    {
        const int i = std::distance(m_systems.cbegin(), it);
        beginRemoveRows(QModelIndex(), i, i);
        m_systems.removeAt(i);
        endRemoveRows();
        emit dataChanged(index(i), index(m_systems.count() - 1));
    }
}

} // namespace nx::vms::client::desktop
