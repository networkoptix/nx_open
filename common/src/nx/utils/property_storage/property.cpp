#include "property.h"

#include "storage.h"

namespace nx::utils::property_storage {

BaseProperty::BaseProperty(
    Storage* storage,
    const QString& name,
    const QString& description)
    :
    storage(storage),
    name(name),
    description(description)
{
    storage->registerProperty(this);
}

void BaseProperty::notify()
{
    emit changed(this);
}

void BaseProperty::save(const QString& value)
{
    storage->writeValue(name, value);
}

} // namespace nx::utils::property_storage
