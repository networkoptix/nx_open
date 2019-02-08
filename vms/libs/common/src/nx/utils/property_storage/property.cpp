#include "property.h"

#include "storage.h"

namespace nx::utils::property_storage {

BaseProperty::BaseProperty(
    Storage* storage,
    const QString& name,
    const QString& description,
    bool secure)
    :
    storage(storage),
    name(name),
    description(description),
    secure(secure)
{
    storage->registerProperty(this);
}

void BaseProperty::notify()
{
    emit changed(this);
}

} // namespace nx::utils::property_storage
