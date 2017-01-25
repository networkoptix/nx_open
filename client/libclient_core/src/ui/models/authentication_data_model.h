#pragma once

#include <QtCore/QAbstractListModel>
#include <QtCore/QUuid>

#include <utils/common/credentials.h>

class AuthenticationDataModel: public QAbstractListModel
{
    Q_OBJECT
    typedef QAbstractListModel base_type;

    Q_PROPERTY(QUuid systemId READ systemId WRITE setSystemId NOTIFY systemIdChanged)
    Q_PROPERTY(QnCredentials defaultCredentials
        READ defaultCredentials NOTIFY defaultCredentialsChanged)

public:
    enum Roles
    {
        CredentialsRole = Qt::UserRole + 1,
        RolesCount
    };

    AuthenticationDataModel(QObject* parent = nullptr);
    ~AuthenticationDataModel();

public:
    QUuid systemId() const;
    void setSystemId(const QUuid& localId);

    QnCredentials defaultCredentials() const;

public:
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void updateData();

signals:
    void systemIdChanged();
    void defaultCredentialsChanged();

private:
    QUuid m_systemId;
    QList<QnCredentials> m_credentialsList;
};
