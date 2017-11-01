
#pragma once

#include <QtCore/QSharedPointer>

#include <network/base_system_description.h>
#include <utils/common/connective.h>

class QnSystemDescriptionAggregator : public Connective<QnBaseSystemDescription>
{
    typedef Connective<QnBaseSystemDescription> base_type;

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

    QnUuid localId() const override;

    QString name() const override;

    QString ownerAccountEmail() const override;

    QString ownerFullName() const override;

    bool isCloudSystem() const override;

    bool isNewSystem() const override;

    bool isRunning() const override;

    bool isReachable() const override;

    bool isConnectable() const override;

    ServersList servers() const override;

    bool isReachableServer(const QnUuid& serverId) const override;

    bool containsServer(const QnUuid& serverId) const override;

    QnModuleInformation getServer(const QnUuid& serverId) const override;

    nx::utils::Url getServerHost(const QnUuid& serverId) const override;

    qint64 getServerLastUpdatedMs(const QnUuid& serverId) const override;

    bool safeMode() const override;

private:
    void emitSystemChanged();

    void onSystemNameChanged(const QnSystemDescriptionPtr& system);

    ServersList gatherServers() const;

    void updateServers();

    void handleServerChanged(const QnUuid& serverId, QnServerFields fields);

    bool isEmptyAggregator() const;

private:
    typedef QMap<int, QnSystemDescriptionPtr> SystemsMap;
    SystemsMap m_systems;
    ServersList m_servers;
};
