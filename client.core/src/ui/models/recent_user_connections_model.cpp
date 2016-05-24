#include "recent_user_connections_model.h"

#include <client/client_recent_connections_manager.h>

#include <core/core_settings.h>
#include <core/user_recent_connection_data.h>

#include <nx/utils/raii_guard.h>
#include <utils/math/math.h>

namespace
{
    enum RoleId
    {
        FirstRoleId = Qt::UserRole + 1

        , UserNameRoleId = FirstRoleId
        , PasswordRoleId
        , HasStoredPasswordRoleId
        , RolesCount
    };

    typedef QHash<int, QByteArray> RoleNameHash;
    const auto kRoleNames = []()-> RoleNameHash
    {
        RoleNameHash result;
        result.insert(UserNameRoleId, "userName");
        result.insert(PasswordRoleId, "password");
        result.insert(HasStoredPasswordRoleId, "hasStoredPassword");
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

QString QnRecentUserConnectionsModel::firstUser() const
{
    if (!hasConnections())
        return QString();

    return m_data.first().userName;
}

void QnRecentUserConnectionsModel::updateData(const QnUserRecentConnectionDataList &newData)
{
    if (m_data == newData)
        return;

    const bool hadConnections = hasConnections();
    const bool firstDataChanged = !m_data.isEmpty() && !newData.isEmpty() && m_data.first() != newData.first();

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

    if (firstDataChanged)
        emit firstUserChanged();
}

int QnRecentUserConnectionsModel::rowCount(const QModelIndex &parent) const
{
    return (parent.isValid() ? 0 : m_data.size());
}

QVariant QnRecentUserConnectionsModel::data(const QModelIndex &index, int role) const
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
        return userPasswordData.userName;
    case PasswordRoleId:
        return userPasswordData.password;
    case HasStoredPasswordRoleId:
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

