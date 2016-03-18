#pragma once

#include <QtCore/QAbstractListModel>

#include <utils/common/connective.h>

class QnLastSystemUsersModel : public Connective<QAbstractListModel>
{
    Q_OBJECT
    typedef Connective<QAbstractListModel> base_type;

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

signals:
    void systemNameChanged();

    void hasConnectionsChanged();

private:
    class LastUsersManager;
    static LastUsersManager &instance();

private:
    typedef QPair<QString, QString> UserPasswordPair;
    typedef QList<UserPasswordPair> UserPasswordPairList;

    void updateData(const UserPasswordPairList &newData);

private:
    QString m_systemName;
    UserPasswordPairList m_data;
};