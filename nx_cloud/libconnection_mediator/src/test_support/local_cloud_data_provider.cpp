/**********************************************************
* Dec 21, 2015
* a.kolesnikov
***********************************************************/

#include "local_cloud_data_provider.h"


namespace nx {
namespace hpm {

boost::optional< AbstractCloudDataProvider::System >
    LocalCloudDataProvider::getSystem(const String& systemId) const
{
    std::unique_lock<std::mutex> lk(m_mutex);
    auto systemIter = m_systems.find(systemId);
    if (systemIter == m_systems.end())
        return boost::none;
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

}   //hpm
}   //nx
