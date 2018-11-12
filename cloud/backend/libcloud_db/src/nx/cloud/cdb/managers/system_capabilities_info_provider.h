#pragma once

#include <nx/utils/subscription.h>
#include <nx/utils/thread/mutex.h>

#include <nx/data_sync_engine/connection_manager.h>

#include "system_manager.h"

namespace nx { namespace data_sync_engine { class ConnectionManager; } }

namespace nx {
namespace cdb {

class SystemCapabilitiesProvider:
    public AbstractSystemExtension
{
public:
    SystemCapabilitiesProvider(
        AbstractSystemManager* systemManager,
        data_sync_engine::ConnectionManager* ec2ConnectionManager);
    virtual ~SystemCapabilitiesProvider();

    virtual void modifySystemBeforeProviding(api::SystemDataEx* system) override;

private:
    mutable QnMutex m_mutex;
    AbstractSystemManager* m_systemManager;
    data_sync_engine::ConnectionManager* m_ec2ConnectionManager;
    nx::utils::SubscriptionId m_systemStatusChangedSubscriptionId;
    std::map<std::string /*system id*/, int /*proto version*/> m_systemIdToProtoVersion;

    void onSystemStatusChanged(
        const std::string& systemId,
        data_sync_engine::SystemStatusDescriptor statusDescription);
};

} // namespace cdb
} // namespace nx
