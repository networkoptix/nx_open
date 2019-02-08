#include "storage.h"

#include <utils/crypt/symmetrical.h>

#include <nx/utils/log/assert.h>

namespace nx::utils::property_storage {

Storage::Storage(AbstractBackend* backend, QObject* parent):
    QObject(parent),
    m_backend(backend)
{
}

void Storage::load()
{
    for (const auto& property: m_properties)
        loadProperty(property);
}

void Storage::sync()
{
    m_backend->sync();
}

void Storage::setSecurityKey(const QByteArray& value)
{
    m_securityKey = value;
}

void Storage::registerProperty(BaseProperty* property)
{
    NX_ASSERT(!m_properties.contains(property->name));
    m_properties[property->name] = property;
    connect(property, &BaseProperty::changed, this, &Storage::saveProperty);
}

void Storage::unregisterProperty(BaseProperty* property)
{
    property->disconnect(this);
    m_properties.remove(property->name);
}

QList<BaseProperty*> Storage::properties() const
{
    return m_properties.values();
}

void Storage::loadProperty(BaseProperty* property)
{
    QString rawValue = readValue(property->name);
    if (property->secure)
        rawValue = decodeStringFromHexStringAES128CBC(rawValue, m_securityKey);
    property->loadSerializedValue(rawValue);
}

void Storage::saveProperty(BaseProperty* property)
{
    QString rawValue = property->serialized();
    if (property->secure)
        rawValue = encodeHexStringFromStringAES128CBC(rawValue, m_securityKey);
    writeValue(property->name, rawValue);
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
