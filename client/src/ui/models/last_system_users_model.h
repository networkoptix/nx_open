#pragma once

#include <QtCore/QAbstractListModel>


typedef QPair<QString, QString> UserPasswordPair;
typedef QList<UserPasswordPair> UserPasswordPairList;

class QnLastSystemUsersModel : public QAbstractListModel
{
    Q_OBJECT
    typedef QAbstractListModel base_type;

    Q_PROPERTY(QString systemName READ systemName WRITE setSystemName NOTIFY systemNameChanged)
    Q_PROPERTY(bool hasConnections READ hasConnections NOTIFY hasConnectionsChanged)

public:
    QnLastSystemUsersModel(QObject *parent = nullptr);

    virtual ~QnLastSystemUsersModel();

public: // properties
    QString systemName() const;

    void setSystemName(const QString &systemName);

    bool hasConnections() const;

public: // overrides
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    QHash<int, QByteArray> roleNames() const;

    void updateData(const UserPasswordPairList &newData);

signals:
    void systemNameChanged();

    void hasConnectionsChanged();


private:
    QString m_systemName;
    UserPasswordPairList m_data;
};