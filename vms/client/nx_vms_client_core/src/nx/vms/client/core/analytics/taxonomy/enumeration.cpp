// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "enumeration.h"

#include <optional>
#include <set>

#include <nx/analytics/taxonomy/abstract_enum_type.h>

using namespace nx::analytics::taxonomy;

namespace nx::vms::client::core::analytics::taxonomy {

struct Enumeration::Private
{
    std::vector<AbstractEnumType*> enumTypes;
    mutable std::optional<std::vector<QString>> cachedItems;
};

Enumeration::Enumeration(QObject* parent):
    QObject(parent),
    d(new Private())
{
}

Enumeration::~Enumeration()
{
    // Required here for forward-declared scoped pointer destruction.
}

void Enumeration::addEnumType(AbstractEnumType* enumType)
{
    d->enumTypes.push_back(enumType);
    d->cachedItems.reset();
}

std::vector<QString> Enumeration::items() const
{
    if (d->cachedItems)
        return *d->cachedItems;

    std::set<QString> mergedItems;
    for (const AbstractEnumType* enumType: d->enumTypes)
    {
        std::vector<QString> enumTypeItems = enumType->items();
        mergedItems.insert(
            std::make_move_iterator(enumTypeItems.begin()),
            std::make_move_iterator(enumTypeItems.end()));
    }

    d->cachedItems = std::vector<QString>(
        std::make_move_iterator(mergedItems.begin()),
        std::make_move_iterator(mergedItems.end()));

    return *d->cachedItems;
}

} // namespace nx::vms::client::core::analytics::taxonomy
