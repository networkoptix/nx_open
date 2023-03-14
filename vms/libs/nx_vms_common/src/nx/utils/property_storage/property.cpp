// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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

bool BaseProperty::exists() const
{
    return storage->exists(name);
}

void BaseProperty::notify()
{
    emit changed(this);
    emit storage->changed(this);
}

} // namespace nx::utils::property_storage
