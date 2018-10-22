#pragma once

#include <unordered_map>
#include <typeinfo>
#include <typeindex>
#include <set>

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <common/common_module.h>
#include <common/common_module_aware.h>

#include <nx/utils/log/log.h>
#include <nx/utils/meta/member_detector.h>

#include <nx/fusion/model_functions.h>

namespace nx::analytics {

namespace details {

using PluginIds = std::set<QString>;

DECLARE_FIELD_DETECTOR(hasPluginIds, pluginIds, QString);

inline QnMediaServerResourcePtr server(QnCommonModule* commonModule)
{
    if (!commonModule)
    {
        NX_ASSERT(false, "Can't access common module");
        return QnMediaServerResourcePtr();
    }

    auto resourcePool = commonModule->resourcePool();
    if (!resourcePool)
    {
        NX_ASSERT(false, "Can't access resource pool");
        return QnMediaServerResourcePtr();
    }

    return resourcePool->getResourceById<QnMediaServerResource>(commonModule->moduleGUID());
}

} // namespace details

class DescriptorListManager: public QnCommonModuleAware
{
    using base_type = QnCommonModuleAware;

    template<typename Descriptor>
    using Container = std::map<QString, Descriptor>;
public:
    DescriptorListManager(QnCommonModule* serverModule);

    template<typename Descriptor>
    void registerType(const QString& propertyName)
    {
        m_typeMap.emplace(typeid(Descriptor), propertyName);
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

        for (const auto& entry: descriptorsToAdd)
        {
            const auto& id = entry.first;
            const auto& descriptor = entry.second;

            auto itr = descriptors.find(id);
            if (itr == descriptors.cend())
                descriptors[descriptor.getId()] = descriptor;

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

        auto currentServerPtr = details::server(commonModule());
        if (!currentServerPtr)
        {
            NX_ASSERT(false, "Can't find current server resource");
            return;
        }

        currentServerPtr->setProperty(
            propertyName(typeid(Descriptor)),
            QString::fromUtf8(QJson::serialized(descriptors)));
    }

    template <typename Descriptor>
    void addDescriptor(const Descriptor& descriptor)
    {
        Container<Descriptor> descriptors;
        descriptors.emplace(descriptor.id, descriptor);
        addDescriptors(descriptors);
    }

private:
    template<typename Descriptor>
    Container<Descriptor> descriptorsUnsafe() const
    {
        auto currentServerPtr = details::server(commonModule());
        auto descriptorsString = currentServerPtr->getProperty(propertyName(typeid(Descriptor)));

        return QJson::deserialized(descriptorsString.toUtf8(), Container<Descriptor>());
    }

private:
    QString propertyName(const std::type_info& typeInfo) const;

private:
    mutable QnMutex m_mutex;
    std::unordered_map<std::type_index, QString> m_typeMap;
};

} // namespace nx::analytics
