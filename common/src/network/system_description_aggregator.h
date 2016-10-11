
#pragma once

#include <QtCore/QSharedPointer>

#include <network/base_system_description.h>

class QnSystemDescriptionAggregator : public QnBaseSystemDescription
{
    typedef QnBaseSystemDescription base_type;

public:
    QnSystemDescriptionAggregator(int priority,
        const QnSystemDescriptionPtr &systemDescription);

    virtual ~QnSystemDescriptionAggregator() = default;

    bool isAggregator() const;

    bool containsSystem(const QString& systemId) const;

    void mergeSystem(int priority, const QnSystemDescriptionPtr& system);

    void removeSystem(int priority);

public: // overrides
    QString id() const override;

    QString name() const override;

    QString ownerAccountEmail() const override;

    QString ownerFullName() const override;

    bool isCloudSystem() const override;

    ServersList servers() const override;

    bool containsServer(const QnUuid& serverId) const override;

    QnModuleInformation getServer(const QnUuid& serverId) const override;

    QUrl getServerHost(const QnUuid& serverId) const override;

    qint64 getServerLastUpdatedMs(const QnUuid& serverId) const override;

private:
    void updateServers(const ServersList& oldServers);

    void emitHeadChanged();

    void onSystemNameChanged(const QnSystemDescriptionPtr& system);

private:
    typedef QMap<int, QnSystemDescriptionPtr> SystemsMap;
    SystemsMap m_systems;
    ServersList m_servers;
};