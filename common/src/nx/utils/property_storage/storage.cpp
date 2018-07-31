#include "storage.h"

#include <nx/utils/log/assert.h>

namespace nx::utils::property_storage {

Storage::Storage(AbstractBackend* backend):
    m_backend(backend)
{
}

void Storage::load()
{
    for (const auto& property: m_properties)
        property->updateValue(readValue(property->name));
}

void Storage::registerProperty(BaseProperty* property)
{
    NX_ASSERT(!m_properties.contains(property->name));
    m_properties[property->name] = property;
}

void Storage::unregisterProperty(BaseProperty* property)
{
    m_properties.remove(property->name);
}

QString Storage::readValue(const QString& name)
{
    return m_backend->readValue(name);
}

void Storage::writeValue(const QString& name, const QString& value)
{
    m_backend->writeValue(name, value);
}

void Storage::removeValue(const QString& name)
{
    m_backend->removeValue(name);
}

} // namespace nx::utils::property_storage
