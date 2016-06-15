#pragma once

#include <QtCore/QAbstractListModel>
#include <core/user_recent_connection_data.h>

class QnRecentUserConnectionsModel : public QAbstractListModel
{
    Q_OBJECT
    typedef QAbstractListModel base_type;

    Q_PROPERTY(QString systemName READ systemName WRITE setSystemName NOTIFY systemNameChanged)
    Q_PROPERTY(bool hasConnections READ hasConnections NOTIFY hasConnectionsChanged)
    Q_PROPERTY(QString firstUser READ firstUser NOTIFY firstUserChanged)

public:
    enum Roles
    {
        FirstRole = Qt::UserRole + 1,

        UserNameRole = FirstRole,
        PasswordRole,
        HasStoredPasswordRole,

        RolesCount
    };

    QnRecentUserConnectionsModel(QObject *parent = nullptr);

    virtual ~QnRecentUserConnectionsModel();

public: // properties
    QString systemName() const;

    void setSystemName(const QString &systemName);

    bool hasConnections() const;

    QString firstUser() const;

public: // overrides
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    QHash<int, QByteArray> roleNames() const override;

    void updateData(const QnUserRecentConnectionDataList &newData);

public slots:
    QVariant getData(const QString &dataRole, int row);

signals:
    void systemNameChanged();
    void hasConnectionsChanged();
    void firstUserChanged();

private:
    QString m_systemName;
    QnUserRecentConnectionDataList m_data;
};
