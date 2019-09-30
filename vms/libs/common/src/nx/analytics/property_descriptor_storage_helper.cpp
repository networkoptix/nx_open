#include "property_descriptor_storage_helper.h"

#include <core/resource/resource.h>

namespace nx::analytics {

PropertyDescriptorStorageHelper::PropertyDescriptorStorageHelper(
    QnResourcePtr resource,
    QString propertyName,
    std::function<void()> propertyChangedHandler)
    :
    m_resource(std::move(resource)),
    m_propertyName(propertyName),
    m_propertyChangedHandler(std::move(propertyChangedHandler))
{
    connect(
        m_resource,
        &QnResource::propertyChanged,
        this,
        [this](const auto& resource, const auto& key)
        {
            if (key != m_propertyName)
                return;

            if (NX_ASSERT(m_propertyChangedHandler))
                m_propertyChangedHandler();
        });
}

} // namespace nx::analytics
