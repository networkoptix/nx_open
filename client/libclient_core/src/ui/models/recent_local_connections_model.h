#pragma once

#include <QtCore/QAbstractListModel>
#include <client_core/local_connection_data.h>

class QnRecentLocalConnectionsModel : public QAbstractListModel
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

    QnRecentLocalConnectionsModel(QObject *parent = nullptr);

    virtual ~QnRecentLocalConnectionsModel();

public: // properties
    QString systemName() const;

    void setSystemName(const QString &systemName);

    bool hasConnections() const;

    QString firstUser() const;

public: // overrides
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    QHash<int, QByteArray> roleNames() const override;

    void updateData(const QnLocalConnectionDataList &newData);

public slots:
    QVariant getData(const QString &dataRole, int row);

signals:
    void systemNameChanged();
    void hasConnectionsChanged();
    void connectionDataChanged(int index);
    void firstUserChanged();

private:
    QString m_systemName;
    QnLocalConnectionDataList m_data;
};
