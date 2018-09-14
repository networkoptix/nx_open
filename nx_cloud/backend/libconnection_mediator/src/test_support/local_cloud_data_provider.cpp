#include "local_cloud_data_provider.h"

namespace nx {
namespace hpm {

std::optional< AbstractCloudDataProvider::System >
    LocalCloudDataProvider::getSystem(const String& systemId) const
{
    std::unique_lock<std::mutex> lk(m_mutex);
    auto systemIter = m_systems.find(systemId);
    if (systemIter == m_systems.end())
        return std::nullopt;
    return systemIter->second;
}

void LocalCloudDataProvider::addSystem(
    String systemId,
    AbstractCloudDataProvider::System systemData)
{
    std::unique_lock<std::mutex> lk(m_mutex);
    m_systems.emplace(
        std::move(systemId),
        std::move(systemData));
}

//-------------------------------------------------------------------------------------------------
// class CloudDataProviderStub

CloudDataProviderStub::CloudDataProviderStub(AbstractCloudDataProvider* target):
    m_target(target)
{
}

std::optional<AbstractCloudDataProvider::System>
    CloudDataProviderStub::getSystem(const String& systemId) const
{
    return m_target->getSystem(systemId);
}

} // namespace hpm
} // namespace nx
