
#include "last_system_users_model.h"

#include <utils/math/math.h>
#include <core/core_settings.h>
#include <core/system_connection_data.h>
#include <nx/utils/raii_guard.h>

namespace
{
    typedef QPointer<QnLastSystemUsersModel> ModelPtr;
    int qHash(const ModelPtr &model)
    {
        return qHash(model.data());
    }

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

//

class QnLastSystemUsersModel::LastUsersManager : public QObject
{
    typedef QObject base_type;

public:
    LastUsersManager();

    ~LastUsersManager();

    void addModel(const ModelPtr &model);

    void removeModel(const ModelPtr &model);

private:
    void updateModelBinding(const ModelPtr &model);

    void updateModelsData();


private:
    typedef QSet<ModelPtr> UnboundModelsSet;
    typedef QHash<QString, ModelPtr> BoundModelsHash;
    typedef QHash<QString, UserPasswordPairList> DataCache;

    UnboundModelsSet m_unbound;
    BoundModelsHash m_bound;
    DataCache m_dataCache;
};

QnLastSystemUsersModel::LastUsersManager::LastUsersManager()
    : base_type()
    , m_unbound()
    , m_bound()
    , m_dataCache()
{
    NX_ASSERT(qnCoreSettings, Q_FUNC_INFO, "Core settings are empty");

    const auto coreSettingsHandler = [this](int id)
    {
        if (id == QnCoreSettings::LastSystemConnections)
            updateModelsData();
    };

    connect(qnCoreSettings, &QnCoreSettings::valueChanged
        , this, coreSettingsHandler);

    updateModelsData();
}

QnLastSystemUsersModel::LastUsersManager::~LastUsersManager()
{
}

void QnLastSystemUsersModel::LastUsersManager::addModel(const ModelPtr &model)
{
    connect(model.data(), &QnLastSystemUsersModel::systemNameChanged
        , this, [this, model]()
    {
        updateModelBinding(model);
    });

    const auto systemName = model->systemName();
    m_unbound.insert(model);
    if (!systemName.isEmpty())
        updateModelBinding(model);
}

void QnLastSystemUsersModel::LastUsersManager::removeModel(const ModelPtr &model)
{
    if (m_unbound.remove(model))
        return;

    const auto systemName = model->systemName();
    const auto it = m_bound.find(systemName);
    const bool isFound = (it != m_bound.end());

    NX_ASSERT(isFound, Q_FUNC_INFO, "Model has not been found");
    if (!isFound)
        return;

    m_bound.erase(it);
}

void QnLastSystemUsersModel::LastUsersManager::updateModelBinding(
    const ModelPtr &model)
{
    const bool isCorrectSystemName = !model->systemName().isEmpty();
    NX_ASSERT(isCorrectSystemName, Q_FUNC_INFO
        , "System name for model can't be empty");
    if (!isCorrectSystemName)
        return;

    const auto itUnbound = m_unbound.find(model);
    const bool isUnbound = (itUnbound != m_unbound.end());
    NX_ASSERT(isUnbound, Q_FUNC_INFO, "Model is not unbound");

    const auto systemName = model->systemName();
    if (isUnbound)
    {
        m_unbound.erase(itUnbound);
    }
    else
    {
        const auto it = std::find_if(m_bound.begin(), m_bound.end()
            , [model](const ModelPtr &modelValue)
        {
            return (model == modelValue);
        });

        const bool isBoundAlready = (it != m_bound.end());
        NX_ASSERT(!isBoundAlready, Q_FUNC_INFO, "Model is bound");
        if (isBoundAlready)
            return;

        const bool modelWithSameSystem =
            (m_bound.find(systemName) != m_bound.end());
        NX_ASSERT(!modelWithSameSystem, Q_FUNC_INFO
            , "Model with the same system name exists");

        if (modelWithSameSystem)
            return;
    }

    m_bound.insert(systemName, model);
    model->updateData(m_dataCache.value(systemName));
}

void QnLastSystemUsersModel::LastUsersManager::updateModelsData()
{
    m_dataCache.clear();
    const auto lastConnectionsData = qnCoreSettings->lastSystemConnections();
    for (const auto connectionDesc : lastConnectionsData)
    {
        m_dataCache[connectionDesc.systemName].append(UserPasswordPair(
            connectionDesc.userName, connectionDesc.password));
    }
    for (const auto boundModel : m_bound)
    {
        const auto systemName = boundModel->systemName();
        boundModel->updateData(m_dataCache.value(systemName));
    }
}

//

QnLastSystemUsersModel::QnLastSystemUsersModel(QObject *parent)
    : base_type(parent)
    , m_systemName()
    , m_data()
{
    instance().addModel(this);
}

QnLastSystemUsersModel::~QnLastSystemUsersModel()
{
    instance().removeModel(this);
}

QnLastSystemUsersModel::LastUsersManager &QnLastSystemUsersModel::instance()
{
    static LastUsersManager inst;
    return inst;
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
