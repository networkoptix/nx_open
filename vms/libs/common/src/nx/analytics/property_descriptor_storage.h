#pragma once

#include <nx/utils/thread/mutex.h>
#include <nx/utils/log/log.h>
#include <nx/utils/data_structures/map_helper.h>
#include <core/resource/resource.h>

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

public:
    PropertyDescriptorStorage(QnResourcePtr serverResource, QString propertyName):
        m_resource(std::move(serverResource)),
        m_propertyName(std::move(propertyName)),
        m_helper(
            m_resource,
            m_propertyName,
            [this]()
            {
                QnMutexLocker lock(&m_mutex);
                m_cached = false;
                m_signalHasBeenCaught = true;
            })
    {
    }

    Container fetch() const
    {
        {
            QnMutexLocker lock(&m_mutex);
            if (m_cached)
                return m_cachedContainer;
        }

        const auto serialized = m_resource->getProperty(m_propertyName);
        if (serialized.isEmpty())
            return Container();

        if (!QJson::deserialize(serialized, &m_cachedContainer))
        {
            NX_WARNING(this, "Unable to deserialize descriptor container from: %1", serialized);
            return Container();
        }

        {
            QnMutexLocker lock(&m_mutex);
            if (!m_signalHasBeenCaught)
                m_cached = true;

            m_signalHasBeenCaught = false;
        }
        return m_cachedContainer;
    }

    void save(const Container& container)
    {
        m_resource->setProperty(
            m_propertyName,
            QString::fromUtf8(QJson::serialized(container)));
        m_resource->saveProperties();
    }

private:
    const QnResourcePtr m_resource;
    const QString m_propertyName;
    PropertyDescriptorStorageHelper m_helper;

    mutable QnMutex m_mutex;
    mutable bool m_cached = false;
    mutable bool m_signalHasBeenCaught = false;
    mutable Container m_cachedContainer;
};

} // namespace nx::analytics
