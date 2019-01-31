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
#include <nx/utils/member_detector.h>
#include <nx/utils/type_traits.h>
#include <nx/utils/std/optional.h>

#include <nx/fusion/model_functions.h>

#include <nx/vms/api/analytics/descriptors.h>
#include <core/resource/camera_resource.h>

namespace nx::analytics {

namespace details {

using PluginIds = std::set<QString>;

NX_UTILS_DECLARE_FIELD_DETECTOR(HasField_paths,
    paths, std::set<nx::vms::api::analytics::HierarchyPath>);

inline QnMediaServerResourcePtr server(
    QnCommonModule* commonModule,
    const QnUuid& serverId = QnUuid())
{
    if (!NX_ASSERT(commonModule, "Can't access common module"))
        return QnMediaServerResourcePtr();

    auto resourcePool = commonModule->resourcePool();
    if (!NX_ASSERT(resourcePool, "Can't access resource pool"))
        return QnMediaServerResourcePtr();

    return resourcePool->getResourceById<QnMediaServerResource>(
        serverId.isNull() ? commonModule->moduleGUID() : serverId);
}

// TODO: #dmishin Make generic getter for supported ids.
template<typename T>
std::set<QString> descriptorIds(const QnVirtualCameraResourcePtr& /*device*/)
{
    static_assert(sizeof(T) < 0, "Method is allowed only for event and object type descriptors");
    return std::set<QString>();
}

template<>
std::set<QString> descriptorIds<nx::vms::api::analytics::EventTypeDescriptor>(
    const QnVirtualCameraResourcePtr& device);

template<>
std::set<QString> descriptorIds<nx::vms::api::analytics::ObjectTypeDescriptor>(
    const QnVirtualCameraResourcePtr& device);

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
    Container<Descriptor> currentServerDescriptors() const
    {
        // TODO: #dmishin need to be moved to server side
        QnMutexLocker lock(&m_mutex);
        return descriptorsUnsafe<Descriptor>(commonModule()->moduleGUID());
    }

    template<typename Descriptor>
    Container<Descriptor> descriptors(const QnMediaServerResourceList& servers)
    {
        QnMutexLocker lock(&m_mutex);
        Container<Descriptor> result;
        for (const auto& server: servers)
        {
            auto serverDescriptors = descriptorsUnsafe<Descriptor>(server->getId());
            result.insert(serverDescriptors.cbegin(), serverDescriptors.cend());
        }

        return result;
    }

    template<typename Descriptor>
    Container<Descriptor> allDescriptorsInTheSystem() const
    {
        QnMutexLocker lock(&m_mutex);
        return allDescriptorsInTheSystemUnsafe<Descriptor>();
    }

    template<typename Descriptor>
    Container<Descriptor> deviceDescriptors(const QnVirtualCameraResourcePtr& device) const
    {
        QnMutexLocker lock(&m_mutex);
        return deviceDescriptorsUnsafe<Descriptor>(device);
    }

    template<typename Descriptor>
    Container<Descriptor> deviceDescriptors(const QnVirtualCameraResourceList& devices) const
    {
        QnMutexLocker lock(&m_mutex);
        Container<Descriptor> result;
        for (const auto& device: devices)
        {
            const auto deviceDescriptors = deviceDescriptorsUnsafe<Descriptor>(device);
            result.insert(deviceDescriptors.cbegin(), deviceDescriptors.cend());
        }

        return result;
    }

    template<typename Descriptor>
    std::optional<Descriptor> descriptor(const QString& id) const
    {
        QnMutexLocker lock(&m_mutex);
        const auto descriptors = allDescriptorsInTheSystemUnsafe<Descriptor>();
        auto itr = descriptors.find(id);
        if (itr == descriptors.cend())
            return std::nullopt;

        return itr->second;
    }

    template<typename Descriptor>
    void addDescriptors(const Container<Descriptor>& descriptorsToAdd)
    {
        QnMutexLocker lock(&m_mutex);
        auto descriptors = descriptorsUnsafe<Descriptor>(/*serverId*/ QnUuid());

        for (const auto& entry: descriptorsToAdd)
        {
            const auto& id = entry.first;
            const auto& descriptor = entry.second;

            auto itr = descriptors.find(id);
            if (itr == descriptors.cend())
            {
                descriptors[descriptor.getId()] = descriptor;
            }
            else if constexpr (details::HasField_paths<Descriptor>::value)
            {
                auto& existingDescriptor = itr->second;
                existingDescriptor.paths.insert(
                    descriptor.paths.cbegin(),
                    descriptor.paths.cend());

                existingDescriptor.item = descriptor.item;
            }
            else
            {
                itr->second = descriptor;
            }
        }

        auto currentServerPtr = details::server(commonModule());
        if (!NX_ASSERT(currentServerPtr, "Can't find current server resource"))
            return;

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

    template <typename Descriptor>
    void clearDescriptors()
    {
        QnMutexLocker lock(&m_mutex);
        auto currentServerPtr = details::server(commonModule());
        if (!NX_ASSERT(currentServerPtr, "Can't find current server resource"))
            return;

        currentServerPtr->setProperty(propertyName(typeid(Descriptor)), QString());
    }

    template<typename Descriptor>
    static std::set<QString> pluginIds(const Container<Descriptor>& descriptors)
    {
        std::set<QString> result;
        for (const auto& entry: descriptors)
        {
            const auto& descriptor = entry.second;
            for (const auto& path: descriptor.paths)
                result.insert(path.pluginId);
        }

        return result;
    }

    template<typename Descriptor>
    static std::set<QString> groupIds(const Container<Descriptor>& descriptors)
    {
        std::set<QString> result;
        for (const auto& entry : descriptors)
        {
            const auto& descriptor = entry.second;
            for (const auto& path: descriptor.paths)
            {
                if (!path.groupId.isEmpty())
                    result.insert(path.groupId);
            }
        }

        return result;
    }

private:
    template<typename Descriptor>
    Container<Descriptor> descriptorsUnsafe(const QnUuid& serverId) const
    {
        const auto server = details::server(commonModule(), serverId);
        const auto descriptorsString = server->getProperty(propertyName(typeid(Descriptor)));

        return QJson::deserialized(descriptorsString.toUtf8(), Container<Descriptor>());
    }

    template<typename Descriptor>
    Container<Descriptor> allDescriptorsInTheSystemUnsafe() const
    {
        Container<Descriptor> result;
        const auto servers = resourcePool()->getAllServers(Qn::AnyStatus);
        for (const auto& server: servers)
        {
            const auto serverDescriptors = descriptorsUnsafe<Descriptor>(server->getId());
            result.insert(serverDescriptors.cbegin(), serverDescriptors.cend());
        }

        return result;
    }

    template<typename Descriptor>
    Container<Descriptor> deviceDescriptorsUnsafe(const QnVirtualCameraResourcePtr& device) const
    {
        Container<Descriptor> result;
        const auto descriptors = allDescriptorsInTheSystemUnsafe<Descriptor>();

        const auto ids = details::descriptorIds<Descriptor>(device);
        for (const auto& id: ids)
        {
            const auto descriptorItr = descriptors.find(id);
            if (descriptorItr != descriptors.cend())
                result.insert(*descriptorItr);
        }

        return result;
    }

private:
    QString propertyName(const std::type_info& typeInfo) const;

private:
    mutable QnMutex m_mutex;
    std::unordered_map<std::type_index, QString> m_typeMap;
};

} // namespace nx::analytics
