#include "recent_local_connections_model.h"

#include <client/client_recent_connections_manager.h>

#include <nx/utils/raii_guard.h>
#include <utils/math/math.h>

namespace
{
    typedef QHash<int, QByteArray> RoleNameHash;
    const auto kRoleNames = []()-> RoleNameHash
    {
        RoleNameHash result;
        result.insert(QnRecentLocalConnectionsModel::UserNameRole, "userName");
        result.insert(QnRecentLocalConnectionsModel::PasswordRole, "password");
        result.insert(QnRecentLocalConnectionsModel::HasStoredPasswordRole, "hasStoredPassword");
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


    // Removes duplicate connection to same system with the same user.
    // Prefers to store connection with saved password
    QnLocalConnectionDataList removeDuplicates(QnLocalConnectionDataList list)
    {
        QnLocalConnectionDataList result;
        while (!list.empty())
        {
            const auto connectionData = *list.begin();
            const auto userName = connectionData.url.userName();
            if (userName.isEmpty())
            {
                list.pop_front();
                continue;
            }

            const auto itStoredPass = std::find_if(list.begin(), list.end(),
                [userName](const QnLocalConnectionData &connection)
                {
                    return !connection.password.isEmpty()
                        && connection.url.userName() == userName;
                });

            /// Stores first found connection with password (if found) or just first connection
            result.append(itStoredPass == list.end() ? connectionData : *itStoredPass);
            const auto newEnd = std::remove_if(list.begin(), list.end(),
                [userName = connectionData.url.userName()](const QnLocalConnectionData &connection)
            {
                return (userName == connection.url.userName());
            });
            list.erase(newEnd, list.end());
        }
        return result;
    }

}

QnRecentLocalConnectionsModel::QnRecentLocalConnectionsModel(QObject *parent):
    base_type(parent),
    m_systemId(),
    m_data()
{
    QnClientRecentConnectionsManager::instance()->addModel(this);
}

QnRecentLocalConnectionsModel::~QnRecentLocalConnectionsModel()
{
    QnClientRecentConnectionsManager::instance()->removeModel(this);
}

QUuid QnRecentLocalConnectionsModel::systemId() const
{
    return m_systemId;
}

void QnRecentLocalConnectionsModel::setSystemId(const QUuid& localId)
{
    if (m_systemId == localId)
        return;

    m_systemId = localId;
    emit systemIdChanged();
}

bool QnRecentLocalConnectionsModel::hasConnections() const
{
    return !m_data.isEmpty();
}

QString QnRecentLocalConnectionsModel::firstUser() const
{
    if (!hasConnections())
        return QString();

    return m_data.first().url.userName();
}

void QnRecentLocalConnectionsModel::updateData(const QnLocalConnectionDataList &newData)
{
    const auto filteredData = removeDuplicates(newData);
    if (m_data == filteredData)
        return;

    const bool hadConnections = hasConnections();
    const bool firstDataChanged = !m_data.isEmpty() && !newData.isEmpty() && m_data.first() != newData.first();

    const auto newCount = filteredData.size();
    for (int newIndex = 0; newIndex != newCount; ++newIndex)
    {
        const auto connectionData = filteredData.at(newIndex);
        const auto it = std::find_if(m_data.begin(), m_data.end(),
            [connectionData](const QnLocalConnectionData &data)
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

    if (m_data.size() > filteredData.size())
    {
        const auto startIndex = filteredData.size();
        const auto finishIndex = m_data.size() - 1;
        beginRemoveRows(QModelIndex(), startIndex, finishIndex);
        m_data.erase(m_data.begin() + startIndex, m_data.end());
        endRemoveRows();
    }

    if (hadConnections != hasConnections())
        emit hasConnectionsChanged();

    if (firstDataChanged)
        emit firstUserChanged();
}

int QnRecentLocalConnectionsModel::rowCount(const QModelIndex &parent) const
{
    return (parent.isValid() ? 0 : m_data.size());
}

QVariant QnRecentLocalConnectionsModel::data(const QModelIndex &index, int role) const
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
            return userPasswordData.password.value();
        case HasStoredPasswordRole:
            return !userPasswordData.password.isEmpty();
        default:
            return QVariant();
    }
}

RoleNameHash QnRecentLocalConnectionsModel::roleNames() const
{
    return kRoleNames;
}

QVariant QnRecentLocalConnectionsModel::getData(const QString &dataRole
    , int row)
{
    const auto it = kRoleIdByName.find(dataRole.toLatin1());
    if (it == kRoleIdByName.end())
        return QVariant();

    return data(index(row), it.value());
}

