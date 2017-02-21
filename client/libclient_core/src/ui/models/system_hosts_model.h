#pragma once

#include <QtCore/QUrl>
#include <QtCore/QAbstractListModel>

#include <network/system_description.h>
#include <utils/common/connective.h>

class QnDisconnectHelper;

class QnSystemHostsModel: public Connective<QAbstractListModel>
{
    Q_OBJECT

    Q_PROPERTY(QString systemId READ systemId WRITE setSystemId NOTIFY systemIdChanged)
    Q_PROPERTY(QUrl firstHost READ firstHost NOTIFY firstHostChanged)

    using base_type = Connective<QAbstractListModel>;

public:
    enum Roles
    {
        UrlRole = Qt::UserRole + 1
    };

    QnSystemHostsModel(QObject *parent = nullptr);

    virtual ~QnSystemHostsModel();

public: // Properties
    QString systemId() const;

    void setSystemId(const QString &id);
    QUrl firstHost() const;

public: // overrides
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    QHash<int, QByteArray> roleNames() const override;

private:
    void reloadHosts();

    void addServer(const QnSystemDescriptionPtr& systemDescription, const QnUuid& serverId);

    typedef QPair<QnUuid, QUrl> ServerIdHostPair;
    typedef QList<ServerIdHostPair> ServerIdHostList;
    void updateServerHost(const QnSystemDescriptionPtr& systemDescription, const QnUuid& serverId);
    bool updateServerHostInternal(const ServerIdHostList::iterator& it, const QUrl& host);

    void removeServer(const QnSystemDescriptionPtr& systemDescription, const QnUuid& serverId);
    void removeServerInternal(const ServerIdHostList::iterator &it);

    ServerIdHostList::iterator getDataIt(const QnUuid &serverId);

signals:
    void systemIdChanged();
    void firstHostChanged();

private:
    typedef QScopedPointer<QnDisconnectHelper> DisconnectHelper;

    DisconnectHelper m_disconnectHelper;
    QString m_systemId;
    ServerIdHostList m_hosts;
};
