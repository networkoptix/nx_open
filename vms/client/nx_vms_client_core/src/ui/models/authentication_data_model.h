// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QAbstractListModel>
#include <QtCore/QUuid>

namespace nx::vms::client::core {

struct CredentialsManager;

struct CredentialsTileModel
{
    QString user;
    QString newPassword;
    bool isPasswordSaved;

private:
    Q_GADGET
    Q_PROPERTY(QString user MEMBER user CONSTANT)
    Q_PROPERTY(QString newPassword MEMBER newPassword CONSTANT)
    Q_PROPERTY(bool isPasswordSaved MEMBER isPasswordSaved CONSTANT)
};

class AuthenticationDataModel: public QAbstractListModel
{
    Q_OBJECT
    typedef QAbstractListModel base_type;

    Q_PROPERTY(QUuid localSystemId
        READ localSystemId
        WRITE setLocalSystemId
        NOTIFY localSystemIdChanged)
    Q_PROPERTY(nx::vms::client::core::CredentialsTileModel defaultCredentials
        READ defaultCredentials NOTIFY defaultCredentialsChanged)
    Q_PROPERTY(QString currentUser
        READ currentUser
        WRITE setCurrentUser
        NOTIFY currentUserChanged)
    Q_PROPERTY(bool hasSavedCredentials
        READ hasSavedCredentials
        NOTIFY hasSavedCredentialsChanged)

public:
    enum Roles
    {
        CredentialsRole = Qt::UserRole + 1,
        RolesCount
    };

    AuthenticationDataModel(QObject* parent = nullptr);
    virtual ~AuthenticationDataModel() override;

public:
    QUuid localSystemId() const;
    void setLocalSystemId(const QUuid& localSystemId);

    CredentialsTileModel defaultCredentials() const;

    QString currentUser() const;
    void setCurrentUser(const QString& value);

    Q_INVOKABLE bool hasSavedCredentials() const;

public:
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void updateData();
    void updateHasSavedCredentials();

signals:
    void localSystemIdChanged();
    void defaultCredentialsChanged();
    void currentUserChanged();
    void hasSavedCredentialsChanged();

private:
    QUuid m_localSystemId;
    QList<CredentialsTileModel> m_credentialsList;
    std::unique_ptr<CredentialsManager> m_credentialsManager;
    QString m_currentUser;
    bool m_hasSavedCredentials = false;
};

} // namespace nx::vms::client::core

Q_DECLARE_METATYPE(nx::vms::client::core::CredentialsTileModel)
