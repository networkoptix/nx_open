#pragma once

#include <nx/analytics/default_descriptor_storage.h>

namespace nx::analytics {

template<typename Descriptor, typename... Scopes>
class DefaultDescriptorStorageFactory
{
    using Storage = DefaultDescriptorStorage<Descriptor, Scopes...>;
public:
    DefaultDescriptorStorageFactory(QString propertyName):
        m_propertyName(std::move(propertyName))
    {
    }

    std::unique_ptr<Storage> operator()(QnResourcePtr resource) const
    {
        return std::make_unique<Storage>(std::move(resource), m_propertyName);
    }

private:
    QString m_propertyName;
};

} // namespace nx::analytics
