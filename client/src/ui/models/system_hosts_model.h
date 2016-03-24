
#pragma once

#include <QtCore/QAbstractListModel>

#include <network/system_description.h>
#include <utils/common/connective.h>

class QnDisconnectHelper;

class QnSystemHostsModel : public Connective<QAbstractListModel>
{
    Q_OBJECT
    typedef Connective<QAbstractListModel> base_type;

    Q_PROPERTY(QString systemId READ systemId WRITE setSystemId NOTIFY systemIdChanged);
    Q_PROPERTY(QString firstHost READ firstHost NOTIFY firstHostChanged);
    Q_PROPERTY(bool isEmpty READ isEmpty NOTIFY isEmptyChanged);

public:
    QnSystemHostsModel(QObject *parent = nullptr);

    virtual ~QnSystemHostsModel();

public: // Properties
    QString systemId() const;

    void setSystemId(const QString &id);

    QString firstHost() const;

    bool isEmpty() const;

public: // overrides
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

private:
    void reloadHosts();

    void addServer(const QnSystemDescriptionPtr &systemDescription
        , const QnUuid &serverId
        , bool tryUpdate);

    bool updateServerHost(const QnSystemDescriptionPtr &systemDescription
        , const QnUuid &serverId);

    void removeServer(const QnSystemDescriptionPtr &systemDescription
        , const QnUuid &serverId);

    typedef QPair<QnUuid, QString> ServerIdHostPair;
    typedef QList<ServerIdHostPair> ServerIdHostList;
    ServerIdHostList::iterator getDataIt(const QnUuid &serverId);

signals:
    void systemIdChanged();

    void firstHostChanged();

    void isEmptyChanged();

private:
    typedef QScopedPointer<QnDisconnectHelper> DisconnectHelper;
    
    DisconnectHelper m_disconnectHelper;
    QString m_systemId;
    ServerIdHostList m_hosts;
};