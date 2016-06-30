
#pragma once

#include <QtCore/QSharedPointer>

#include <network/base_system_description.h>

class QnSystemDescriptionAggregator : public QnBaseSystemDescription
{
    typedef QnBaseSystemDescription base_type;

public:
    QnSystemDescriptionAggregator(const QnSystemDescriptionPtr &systemDescription);

    virtual ~QnSystemDescriptionAggregator();

    bool isAggregator() const;

    bool containsSystem(const QString& systemId) const;

    void mergeSystem(const QnSystemDescriptionPtr& system);

    void removeSystem(const QString& id);

public: // overrides
    QString id() const override;

    QString name() const override;

    QString ownerAccountEmail() const override;

    QString ownerFullName() const override;

    bool isCloudSystem() const override;

    ServersList servers() const override;

    bool containsServer(const QnUuid &serverId) const override;

    QnModuleInformation getServer(const QnUuid &serverId) const override;

    QString getServerHost(const QnUuid &serverId) const override;

    qint64 getServerLastUpdatedMs(const QnUuid &serverId) const override;

private:
    void emitChangesSignals(bool wasCloudSystem
        , const ServersList& oldServers);

private:
    QnSystemDescriptionPtr m_cloudSystem;
    QnSystemDescriptionPtr m_localSystem;

    ServersList m_servers;
};