#pragma once

#include <QtCore/QAbstractListModel>
#include <client_core/local_connection_data.h>

class QnRecentLocalConnectionsModel : public QAbstractListModel
{
    Q_OBJECT
    typedef QAbstractListModel base_type;

    Q_PROPERTY(QUuid systemId READ systemId WRITE setSystemId NOTIFY systemIdChanged)
    Q_PROPERTY(bool hasConnections READ hasConnections NOTIFY hasConnectionsChanged)
    Q_PROPERTY(QString firstUser READ firstUser NOTIFY firstUserChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

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
    QUuid systemId() const;

    void setSystemId(const QUuid& localId);

    bool hasConnections() const;

    QString firstUser() const;

    int count() const;

public: // overrides
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    QHash<int, QByteArray> roleNames() const override;

    void updateData(const QnLocalConnectionDataList &newData);

public slots:
    QVariant getData(const QString &dataRole, int row);

signals:
    void systemIdChanged();
    void hasConnectionsChanged();
    void connectionDataChanged(int index);
    void firstUserChanged();
    void countChanged();

private:
    QUuid m_systemId;
    QnLocalConnectionDataList m_data;
};
