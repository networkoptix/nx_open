#include "last_system_users_model.h"

#include <client/client_recent_connections_manager.h>

#include <core/core_settings.h>
#include <core/system_connection_data.h>

#include <nx/utils/raii_guard.h>
#include <utils/math/math.h>

namespace
{
    enum RoleId
    {
        FirstRoleId = Qt::UserRole + 1

        , UserNameRoleId = FirstRoleId
        , PasswordRoleId

        , RolesCount
    };

    typedef QHash<int, QByteArray> RoleNameHash;
    const auto kRoleNames = []()-> RoleNameHash
    {
        RoleNameHash result;
        result.insert(UserNameRoleId, "userName");
        result.insert(PasswordRoleId, "password");
        return result;
    }();
}

QnLastSystemUsersModel::QnLastSystemUsersModel(QObject *parent)
    : base_type(parent)
    , m_systemName()
    , m_data()
{
    QnClientRecentConnectionsManager::instance()->addModel(this);
}

QnLastSystemUsersModel::~QnLastSystemUsersModel()
{
    QnClientRecentConnectionsManager::instance()->removeModel(this);
}

QString QnLastSystemUsersModel::systemName() const
{
    return m_systemName;
}

void QnLastSystemUsersModel::setSystemName(const QString &systemName)
{
    if (m_systemName == systemName)
        return;

    m_systemName = systemName;
    emit systemNameChanged();
}

bool QnLastSystemUsersModel::hasConnections() const
{
    return !m_data.isEmpty();
}

void QnLastSystemUsersModel::updateData(const UserPasswordPairList &newData)
{
    if (m_data == newData)
        return;

    if (systemName().contains(lit("YURIY")))
        int i = 0;
    const bool hadConnections = hasConnections();

    auto itCurrent = m_data.begin();
    for (auto &newDataValue : newData)
    {
        if (itCurrent == m_data.end())
        {
            const int startIndex = m_data.size();
            const int finishIndex = newData.size() - 1;

            beginInsertRows(QModelIndex(), startIndex, finishIndex);
            const auto endInsertRowsGuard = QnRaiiGuard::createDestructable(
                [this]() { endInsertRows(); });

            for (auto it = newData.begin() + startIndex; it != newData.end(); ++it)
                m_data.append(*it);

            itCurrent = m_data.end();
            break;
        }

        auto &oldDataValue = *itCurrent;
        if (oldDataValue != newDataValue)
        {
            oldDataValue = newDataValue;
            const auto modelIndex = index(itCurrent - m_data.begin());
            dataChanged(modelIndex, modelIndex);
        }

        ++itCurrent;
    }

    if (itCurrent != m_data.end())
    {
        const int startIndex = (itCurrent - m_data.begin());
        const int finishIndex = m_data.size();

        beginRemoveRows(QModelIndex(), startIndex, finishIndex);
        const auto emdRemoveRowsGuard = QnRaiiGuard::createDestructable(
            [this]() { endRemoveRows(); });

        m_data.erase(itCurrent, m_data.end());
    }

    if (hadConnections != hasConnections())
        emit hasConnectionsChanged();
}

int QnLastSystemUsersModel::rowCount(const QModelIndex &parent) const
{
    return (parent.isValid() ? 0 : m_data.size());
}

QVariant QnLastSystemUsersModel::data(const QModelIndex &index, int role) const
{
    const int row = index.row();
    if (!qBetween(0, row, rowCount())
        || !qBetween<int>(FirstRoleId, role, RolesCount))
    {
        return QVariant();
    }

    const auto userPasswordData = m_data.at(row);
    switch (role)
    {
    case UserNameRoleId:
        return userPasswordData.first;
    case PasswordRoleId:
        return userPasswordData.second;
    default:
        return QVariant();
    }
}

RoleNameHash QnLastSystemUsersModel::roleNames() const
{
    return kRoleNames;
}
