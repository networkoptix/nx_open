#include "system_capabilities_info_provider.h"

namespace nx::cloud::db {

constexpr int kMinEc2ProtocolVersionWithCloudMergeSupport = 3041;

SystemCapabilitiesProvider::SystemCapabilitiesProvider(
    AbstractSystemManager* systemManager,
    clusterdb::engine::ConnectionManager* ec2ConnectionManager)
    :
    m_systemManager(systemManager),
    m_ec2ConnectionManager(ec2ConnectionManager)
{
    using namespace std::placeholders;

    m_systemManager->addExtension(this);

    m_ec2ConnectionManager->systemStatusChangedSubscription().subscribe(
        std::bind(&SystemCapabilitiesProvider::onSystemStatusChanged, this, _1, _2),
        &m_systemStatusChangedSubscriptionId);
}

SystemCapabilitiesProvider::~SystemCapabilitiesProvider()
{
    m_ec2ConnectionManager->systemStatusChangedSubscription().removeSubscription(
        m_systemStatusChangedSubscriptionId);

    m_systemManager->removeExtension(this);
}

void SystemCapabilitiesProvider::modifySystemBeforeProviding(
    api::SystemDataEx* system)
{
    QnMutexLocker lock(&m_mutex);

    auto it = m_systemIdToProtoVersion.find(system->id);
    if (it != m_systemIdToProtoVersion.end() &&
        it->second >= kMinEc2ProtocolVersionWithCloudMergeSupport)
    {
        system->capabilities.push_back(api::SystemCapabilityFlag::cloudMerge);
    }
}

void SystemCapabilitiesProvider::onSystemStatusChanged(
    const std::string& systemId,
    clusterdb::engine::SystemStatusDescriptor statusDescription)
{
    QnMutexLocker lock(&m_mutex);

    if (statusDescription.isOnline)
        m_systemIdToProtoVersion[systemId] = statusDescription.protoVersion;
    else
        m_systemIdToProtoVersion.erase(systemId);
}

} // namespace nx::cloud::db
