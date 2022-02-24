// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "authentication_data_model.h"

#include <nx/network/http/auth_tools.h>
#include <nx/vms/client/core/network/credentials_manager.h>

namespace {

using nx::vms::client::core::AuthenticationDataModel;

const auto kRoleNames = QHash<int, QByteArray>{
    {AuthenticationDataModel::CredentialsRole, "credentials"}};

} // namespace

namespace nx::vms::client::core {

AuthenticationDataModel::AuthenticationDataModel(QObject* parent):
    base_type(parent),
    m_credentialsManager(std::make_unique<CredentialsManager>())
{
    connect(this, &AuthenticationDataModel::modelReset,
        this, &AuthenticationDataModel::defaultCredentialsChanged);

    connect(m_credentialsManager.get(), &CredentialsManager::storedCredentialsChanged,
        this, &AuthenticationDataModel::updateData);
}

AuthenticationDataModel::~AuthenticationDataModel()
{
}

QUuid AuthenticationDataModel::localSystemId() const
{
    return m_localSystemId;
}

void AuthenticationDataModel::setLocalSystemId(const QUuid& localSystemId)
{
    if (m_localSystemId == localSystemId)
        return;

    m_localSystemId = localSystemId;
    emit localSystemIdChanged();

    updateData();
}

CredentialsTileModel AuthenticationDataModel::defaultCredentials() const
{
    if (m_credentialsList.isEmpty())
        return {};

    return m_credentialsList.first();
}

void AuthenticationDataModel::updateData()
{
    beginResetModel();

    m_credentialsList.clear();
    const auto storedCredentials = CredentialsManager::credentials(m_localSystemId);
    for (const nx::network::http::Credentials& credentials: storedCredentials)
    {
        if (credentials.username.empty())
            continue;

        CredentialsTileModel model;
        model.user = QString::fromStdString(credentials.username);
        model.isPasswordSaved = !credentials.authToken.empty();
        m_credentialsList.push_back(std::move(model));
    }

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
    auto names = base_type::roleNames();
    names.insert(kRoleNames);
    return names;
}

} // namespace nx::vms::client::core
