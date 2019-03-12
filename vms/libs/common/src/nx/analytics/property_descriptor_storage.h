#pragma once

#include <nx/utils/log/log.h>
#include <nx/utils/data_structures/map_helper.h>
#include <core/resource/resource.h>

#include <nx/fusion/model_functions.h>

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
        m_propertyName(std::move(propertyName))
    {
    }

    Container fetch() const
    {
        const auto serialized = m_resource->getProperty(m_propertyName);
        if (serialized.isEmpty())
            return Container();

        Container container;
        if (!QJson::deserialize(serialized, &container))
        {
            NX_WARNING(this, "Unable to deserialize descriptor container from: %1", serialized);
            return Container();
        }

        return container;
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
};

} // namespace nx::analytics
