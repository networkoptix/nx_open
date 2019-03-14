#include "authentication_data_model.h"

#include <nx/vms/client/core/settings/client_core_settings.h>
#include <client_core/client_core_settings.h>

namespace {

using nx::vms::client::core::AuthenticationDataModel;

const auto kRoleNames = QHash<int, QByteArray>{
    {AuthenticationDataModel::CredentialsRole, "credentials"}};

} // namespace

namespace nx::vms::client::core {

AuthenticationDataModel::AuthenticationDataModel(QObject* parent):
    base_type(parent)
{
    connect(this, &AuthenticationDataModel::modelReset,
        this, &AuthenticationDataModel::defaultCredentialsChanged);

    connect(&settings()->systemAuthenticationData, &Settings::BaseProperty::changed,
        this, &AuthenticationDataModel::updateData);
}

AuthenticationDataModel::~AuthenticationDataModel()
{
}

QUuid AuthenticationDataModel::systemId() const
{
    return m_systemId;
}

void AuthenticationDataModel::setSystemId(const QUuid& localId)
{
    if (m_systemId == localId)
        return;

    m_systemId = localId;
    emit systemIdChanged();

    updateData();
}

nx::vms::common::Credentials AuthenticationDataModel::defaultCredentials() const
{
    if (m_credentialsList.isEmpty())
        return {};

    return m_credentialsList.first();
}

void AuthenticationDataModel::updateData()
{
    beginResetModel();
    m_credentialsList = settings()->systemAuthenticationData()[m_systemId];
    endResetModel();
}

int AuthenticationDataModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_credentialsList.size();
}

QVariant AuthenticationDataModel::data(const QModelIndex& index, int role) const
{
    if (!hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    const auto credentials = m_credentialsList[index.row()];

    switch (role)
    {
        case Qt::DisplayRole:
            return credentials.user;
        case CredentialsRole:
            return QVariant::fromValue(credentials);
        default:
            return QVariant();
    }
}

QHash<int, QByteArray> AuthenticationDataModel::roleNames() const
{
    return base_type::roleNames().unite(kRoleNames);
}

} // namespace nx::vms::client::core
