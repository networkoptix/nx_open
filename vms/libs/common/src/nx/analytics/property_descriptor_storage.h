#pragma once

#include <nx/utils/thread/mutex.h>
#include <nx/utils/log/log.h>
#include <nx/utils/data_structures/map_helper.h>
#include <core/resource/resource.h>
#include <nx/utils/value_cache.h>

#include <nx/fusion/model_functions.h>

#include <nx/analytics/property_descriptor_storage_helper.h>

namespace nx::analytics {

template<typename DescriptorType, typename... Scopes>
class PropertyDescriptorStorage
{

public:
    using Descriptor = DescriptorType;
    using Container = nx::utils::data_structures::MapHelper::NestedMap<
        std::map,
        Scopes...,
        DescriptorType>;
    using NotifyWhenUpdated = std::function<void()>;
public:
    PropertyDescriptorStorage(
        QnResourcePtr serverResource,
        QString propertyName,
        NotifyWhenUpdated notifier)
        :
        m_resource(std::move(serverResource)),
        m_propertyName(std::move(propertyName)),
        m_cachedContainer([this]() { return fetchInternal(); }),
        m_notifier(std::move(notifier)),
        m_helper(
            m_resource,
            m_propertyName,
            [this]()
            {
                m_cachedContainer.reset();
                m_notifier();
            })
    {
    }

    Container fetch() const
    {
        return m_cachedContainer.get();
    }

    void save(const Container& container)
    {
        m_resource->setProperty(
            m_propertyName,
            QString::fromUtf8(QJson::serialized(container)));
        m_resource->saveProperties();
    }

private:
    Container fetchInternal() const
    {
        const auto serialized = m_resource->getProperty(m_propertyName);
        if (serialized.isEmpty())
            return Container();

        Container container;
        if (!QJson::deserialize(serialized, &container))
            NX_WARNING(this, "Unable to deserialize descriptor container from: %1", serialized);

        return container;
    }

private:
    const QnResourcePtr m_resource;
    const QString m_propertyName;

    mutable QnMutex m_mutex;
    mutable nx::utils::CachedValue<Container> m_cachedContainer;

    NotifyWhenUpdated m_notifier;
    PropertyDescriptorStorageHelper m_helper;
};

} // namespace nx::analytics
