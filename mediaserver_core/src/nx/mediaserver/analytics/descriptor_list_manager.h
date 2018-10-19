#pragma once

#include <unordered_map>
#include <typeinfo>
#include <typeindex>
#include <set>

#include <nx/mediaserver/server_module_aware.h>
#include <media_server/media_server_module.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <common/common_module.h>

#include <nx/utils/log/log.h>
#include <nx/utils/meta/member_detector.h>

#include <nx/fusion/model_functions.h>

namespace nx::mediaserver::analytics {

namespace details {

using PluginIds = std::set<QString>;

DECLARE_FIELD_DETECTOR(hasPluginIds, pluginIds, QString);

QnMediaServerResourcePtr server(QnMediaServerModule* serverModule)
{
    if (!serverModule)
    {
        NX_ASSERT(false, "Can't access server module");
        return nullptr;
    }

    auto commonModule = serverModule->commonModule();
    if (!commonModule)
    {
        NX_ASSERT(false, "Can't access common module");
        return nullptr;
    }

    auto resourcePool = serverModule->resourcePool();
    if (!resourcePool)
    {
        NX_ASSERT(false, "Can't access resource pool");
        return nullptr;
    }

    return resourcePool->getResourceById<QnMediaServerResource>(commonModule->moduleGUID());
}

} // namespace details

class DescriptorListManager: public nx::mediaserver::ServerModuleAware
{
    using base_type = nx::mediaserver::ServerModuleAware;

    template<typename Descriptor>
    using Container = std::vector<Descriptor>;
public:
    DescriptorListManager(QnMediaServerModule* serverModule);

    template<typename Descriptor>
    void registerType(const QString& propertyName)
    {
        m_typeInfo.emplace(typeid(Descriptor), propertyName);
    }

    template<typename Descriptor>
    Container<Descriptor> descriptors() const
    {
        QnMutexLocker lock(&m_mutex);
        return descriptorsUnsafe<Descriptor>();
    }

    template<typename Descriptor>
    void addDescriptors(const Container<Descriptor>& descriptorsToAdd)
    {
        QnMutexLocker lock(&m_mutex);
        auto descriptors = descriptorsUnsafe<Descriptor>();

        for (const auto& descriptor: descriptorsToAdd)
        {
            auto itr = descriptors.find(descriptor.id);
            if (itr == descriptors.cend())
            {
                descriptors[descriptor.id] = descriptor;
            }
            else if constexpr (details::hasPluginIds<Descriptor>::value)
            {
                auto& existingDescriptor = itr->second;
                existingDescriptor.pluginsIds.insert(
                    descriptor.pluginIds.cbegin(),
                    descriptor.pluginIds.cend());

                existingDescriptor.name = descriptor.name;
            }
            else
            {
                itr->second = descriptor;
            }
        }

        auto currentServerPtr = server(serverModule());
        if (!currentServerPtr)
        {
            NX_ASSERT(false, "Can't find current server resource");
            return;
        }

        currentServerPtr->setProperty(propertyName(), QnJson::serialized(descriptors));
    }

private:
    template<typename Descriptor>
    Container<Descriptor> descriptorsUnsafe() const
    {
        auto currentServerPtr = server(serverModule());
        auto descriptorsString = currentServerPtr->getProperty(propertyName(typeid(Descriptor)));

        return QnJson::deserialized(descriptorsString.toUtf8(), Container<Descriptor>());
    }

private:
    QString propertyName(const std::type_info& typeInfo) const;

private:
    mutable QnMutex m_mutex;
    std::unordered_map<std::type_index, QString> m_typeMap;
};

} // namespace nx::mediaserver::analytics
