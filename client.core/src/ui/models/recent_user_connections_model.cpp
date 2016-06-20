#include "recent_user_connections_model.h"

#include <client/client_recent_connections_manager.h>

#include <client_core/client_core_settings.h>

#include <nx/utils/raii_guard.h>
#include <utils/math/math.h>

namespace
{
    typedef QHash<int, QByteArray> RoleNameHash;
    const auto kRoleNames = []()-> RoleNameHash
    {
        RoleNameHash result;
        result.insert(QnRecentUserConnectionsModel::UserNameRole, "userName");
        result.insert(QnRecentUserConnectionsModel::PasswordRole, "password");
        result.insert(QnRecentUserConnectionsModel::HasStoredPasswordRole, "hasStoredPassword");
        return result;
    }();

    typedef QHash<QByteArray, int> NameRoleHash;
    const auto kRoleIdByName = []()->NameRoleHash
    {
        NameRoleHash result;
        for (auto it = kRoleNames.begin(); it != kRoleNames.end(); ++it)
            result.insert(it.value(), it.key());
        return result;
    }();
}

QnRecentUserConnectionsModel::QnRecentUserConnectionsModel(QObject *parent)
    : base_type(parent)
    , m_systemName()
    , m_data()
{
    QnClientRecentConnectionsManager::instance()->addModel(this);
}

QnRecentUserConnectionsModel::~QnRecentUserConnectionsModel()
{
    QnClientRecentConnectionsManager::instance()->removeModel(this);
}

QString QnRecentUserConnectionsModel::systemName() const
{
    return m_systemName;
}

void QnRecentUserConnectionsModel::setSystemName(const QString &systemName)
{
    if (m_systemName == systemName)
        return;

    m_systemName = systemName;
    emit systemNameChanged();
}

bool QnRecentUserConnectionsModel::hasConnections() const
{
    return !m_data.isEmpty();
}

void QnRecentUserConnectionsModel::updateData(const QnUserRecentConnectionDataList &newData)
{
    if (m_data == newData)
        return;

    const bool hadConnections = hasConnections();
    
    const auto newCount = newData.size();
    for (int newIndex = 0; newIndex != newCount; ++newIndex)
    {
        const auto connectionData = newData.at(newIndex);
        const auto it = std::find_if(m_data.begin(), m_data.end(), 
            [connectionData](const QnUserRecentConnectionData &data)
        {
            return (data.url.userName() == connectionData.url.userName());
        });

        if (it == m_data.end())
        {
            // Element not found - just insert it
            beginInsertRows(QModelIndex(), newIndex, newIndex);
            m_data.insert(newIndex, connectionData);
            endInsertRows();
        }
        else
        {
            const auto oldIndex = (it - m_data.begin());
            const auto data = m_data.at(oldIndex);
            const int diff = (oldIndex < newIndex ? -1 : 0);
            if (oldIndex != newIndex)
            {
                // Move element to proper position
                beginMoveRows(QModelIndex(), oldIndex, oldIndex, QModelIndex(), newIndex);

                m_data.erase(m_data.begin() + oldIndex);
                m_data.insert(newIndex + diff, connectionData);
                endMoveRows();
            }
            else if (data != connectionData)
            {
                m_data[newIndex + diff] = connectionData;
                emit connectionDataChanged(newIndex + diff);
            }
        }
    }

    if (m_data.size() > newData.size())
    {
        const auto startIndex = newData.size();
        const auto finishIndex = m_data.size() - 1;
        beginRemoveRows(QModelIndex(), startIndex, finishIndex);
        m_data.erase(m_data.begin() + startIndex, m_data.end());
        endRemoveRows();
    }

    if (hadConnections != hasConnections())
        emit hasConnectionsChanged();
}

int QnRecentUserConnectionsModel::rowCount(const QModelIndex &parent) const
{
    return (parent.isValid() ? 0 : m_data.size());
}

QVariant QnRecentUserConnectionsModel::data(const QModelIndex &index, int role) const
{
    const int row = index.row();
    if (!qBetween(0, row, rowCount())
        || !qBetween<int>(FirstRole, role, RolesCount))
    {
        return QVariant();
    }

    const auto userPasswordData = m_data.at(row);
    switch (role)
    {
        case UserNameRole:
            return userPasswordData.url.userName();
        case PasswordRole:
            return userPasswordData.url.password();
        case HasStoredPasswordRole:
            return userPasswordData.isStoredPassword;
        default:
            return QVariant();
    }
}

RoleNameHash QnRecentUserConnectionsModel::roleNames() const
{
    return kRoleNames;
}

QVariant QnRecentUserConnectionsModel::getData(const QString &dataRole
    , int row)
{
    const auto it = kRoleIdByName.find(dataRole.toLatin1());
    if (it == kRoleIdByName.end())
        return QVariant();

    return data(index(row), it.value());
}

