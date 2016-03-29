/**********************************************************
* Dec 21, 2015
* a.kolesnikov
***********************************************************/

#ifndef LOCAL_CLOUD_DATA_PROVIDER_H
#define LOCAL_CLOUD_DATA_PROVIDER_H

#include <map>
#include <mutex>

#include "../cloud_data_provider.h"


namespace nx {
namespace hpm {

class LocalCloudDataProvider
:
    public AbstractCloudDataProvider
{
public:
    virtual boost::optional< AbstractCloudDataProvider::System >
        getSystem(const String& systemId) const override;

    void addSystem(
        String systemId,
        AbstractCloudDataProvider::System systemData);

private:
    mutable std::mutex m_mutex;
    std::map<String, AbstractCloudDataProvider::System> m_systems;
};

class CloudDataProviderStub
:
    public AbstractCloudDataProvider
{
public:
    CloudDataProviderStub(AbstractCloudDataProvider* target)
    :
        m_target(target)
    {
    }

    virtual boost::optional< AbstractCloudDataProvider::System >
        getSystem(const String& systemId) const
    {
        return m_target->getSystem(systemId);
    }

private:
    AbstractCloudDataProvider* m_target;
};

}   //hpm
}   //nx

#endif  //LOCAL_CLOUD_DATA_PROVIDER_H
