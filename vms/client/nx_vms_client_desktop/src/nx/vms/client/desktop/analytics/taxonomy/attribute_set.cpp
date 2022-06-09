// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "attribute_set.h"

#include <optional>

#include "utils.h"

namespace nx::vms::client::desktop::analytics::taxonomy {

struct AttributeSet::Private
{
    std::vector<const nx::analytics::taxonomy::AbstractObjectType*> objectTypes;
    mutable std::optional<std::vector<AbstractAttribute*>> cachedAttributes;
};

AttributeSet::AttributeSet(QObject* parent):
    AbstractAttributeSet(parent),
    d(new Private())
{
}

AttributeSet::~AttributeSet()
{
    // Required here for forward-declared scoped pointer destruction.
}

std::vector<AbstractAttribute*> AttributeSet::attributes() const
{
    if (!d->cachedAttributes)
        d->cachedAttributes = resolveAttributes(d->objectTypes, parent());

    return *d->cachedAttributes;
}

void AttributeSet::addObjectType(nx::analytics::taxonomy::AbstractObjectType* objectType)
{
    d->objectTypes.push_back(objectType);
    d->cachedAttributes.reset();
}

} // namespace nx::vms::client::desktop::analytics::taxonomy
