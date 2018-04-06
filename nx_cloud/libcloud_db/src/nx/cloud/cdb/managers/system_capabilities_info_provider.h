#pragma once

#include <nx/utils/subscription.h>
#include <nx/utils/thread/mutex.h>

#include <nx/data_sync_engine/connection_manager.h>

#include "system_manager.h"

namespace nx {
namespace cdb {

namespace ec2 { class ConnectionManager; }

class SystemCapabilitiesProvider:
    public AbstractSystemExtension
{
public:
    SystemCapabilitiesProvider(
        AbstractSystemManager* systemManager,
        ec2::ConnectionManager* ec2ConnectionManager);
    virtual ~SystemCapabilitiesProvider();

    virtual void modifySystemBeforeProviding(api::SystemDataEx* system) override;

private:
    mutable QnMutex m_mutex;
    AbstractSystemManager* m_systemManager;
    ec2::ConnectionManager* m_ec2ConnectionManager;
    nx::utils::SubscriptionId m_systemStatusChangedSubscriptionId;
    std::map<std::string /*system id*/, int /*proto version*/> m_systemIdToProtoVersion;

    void onSystemStatusChanged(
        const std::string& systemId,
        ec2::SystemStatusDescriptor statusDescription);
};

} // namespace cdb
} // namespace nx
