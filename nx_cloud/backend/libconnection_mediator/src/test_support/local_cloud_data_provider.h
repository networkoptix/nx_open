#pragma once

#include <map>
#include <mutex>

#include "../cloud_data_provider.h"

namespace nx {
namespace hpm {

class LocalCloudDataProvider:
    public AbstractCloudDataProvider
{
public:
    virtual std::optional< AbstractCloudDataProvider::System >
        getSystem(const String& systemId) const override;

    void addSystem(
        String systemId,
        AbstractCloudDataProvider::System systemData);

private:
    mutable std::mutex m_mutex;
    std::map<String, AbstractCloudDataProvider::System> m_systems;
};

class CloudDataProviderStub:
    public AbstractCloudDataProvider
{
public:
    CloudDataProviderStub(AbstractCloudDataProvider* target);

    virtual std::optional< AbstractCloudDataProvider::System >
        getSystem(const String& systemId) const;

private:
    AbstractCloudDataProvider* m_target;
};

} // namespace hpm
} // namespace nx
