#pragma once

#include <nx/utils/subscription.h>
#include <nx/utils/thread/mutex.h>

#include <nx/clusterdb/engine/connection_manager.h>

#include "system_manager.h"

namespace nx::clusterdb::engine { class ConnectionManager; }

namespace nx::cloud::db {

class SystemCapabilitiesProvider:
    public AbstractSystemExtension
{
public:
    SystemCapabilitiesProvider(
        AbstractSystemManager* systemManager,
        clusterdb::engine::ConnectionManager* ec2ConnectionManager);
    virtual ~SystemCapabilitiesProvider();

    virtual void modifySystemBeforeProviding(api::SystemDataEx* system) override;

private:
    mutable QnMutex m_mutex;
    AbstractSystemManager* m_systemManager;
    clusterdb::engine::ConnectionManager* m_ec2ConnectionManager;
    nx::utils::SubscriptionId m_systemStatusChangedSubscriptionId;
    std::map<std::string /*system id*/, int /*proto version*/> m_systemIdToProtoVersion;

    void onSystemStatusChanged(
        const std::string& systemId,
        clusterdb::engine::SystemStatusDescriptor statusDescription);
};

} // namespace nx::cloud::db
