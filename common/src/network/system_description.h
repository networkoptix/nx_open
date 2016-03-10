
#pragma once

#include <QtCore/QObject>

#include <network/module_information.h>
#include <nx/utils/uuid.h>
#include <nx/network/socket_common.h>

class QnSystemDescription;
typedef QSharedPointer<QnSystemDescription> QnSystemDescriptionPtr;

enum class QnServerField
{
    NoField                 = 0x0
    , NameField             = 0x1
    , SystemNameField       = 0x2
    , PrimaryAddressField   = 0x4
};
Q_DECLARE_FLAGS(QnServerFields, QnServerField)

class QnSystemDescription : public QObject
{
    Q_OBJECT
    typedef QObject base_type;

public:
    static QnSystemDescriptionPtr createLocalSystem(const QnUuid &systemId
        , const QString &systemName);

    static QnSystemDescriptionPtr createCloudSystem(const QnUuid &systemId
        , const QString &systemName);

    virtual ~QnSystemDescription();

    QnUuid id() const;

    QString name() const;

    bool isCloudSystem() const;

    typedef QList<QnModuleInformation> ServersList;
    ServersList servers() const;
    
    void addServer(const QnModuleInformation &serverInfo);

    bool containsServer(const QnUuid &serverId) const;

    QnModuleInformation getServer(const QnUuid &serverId) const;

    void updateServer(const QnModuleInformation &serverInfo);

    void removeServer(const QnUuid &serverId);

    void setPrimaryAddress(const QnUuid &serverId
        , const SocketAddress &address);

    SocketAddress getServerPrimaryAddress(const QnUuid &serverId) const;

signals:
    void serverAdded(const QnUuid &serverId);

    void serverRemoved(const QnUuid &serverId);

    void serverChanged(const QnUuid &serverId
        , QnServerFields flags);

private:
    QnSystemDescription(const QnUuid &systemId
        , const QString &systemName
        , const bool isCloudSystem);

private:
    typedef QHash<QnUuid, QnModuleInformation> ServerInfoHash;
    typedef QHash<QnUuid, SocketAddress> PrimaryAddressHash;

    const QnUuid m_id;
    const QString m_systemName;
    const bool m_isCloudSystem;
    ServerInfoHash m_servers;
    PrimaryAddressHash m_primaryAddresses;


};