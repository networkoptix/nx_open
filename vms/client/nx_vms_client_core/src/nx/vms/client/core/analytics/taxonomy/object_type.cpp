// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "object_type.h"

#include <map>
#include <optional>

#include <QtCore/QPointer>

#include <nx/analytics/taxonomy/abstract_object_type.h>
#include <nx/analytics/taxonomy/common.h>
#include <nx/vms/client/core/analytics/taxonomy/abstract_state_view_filter.h>
#include <nx/vms/client/core/analytics/taxonomy/attribute.h>
#include <nx/vms/client/core/analytics/taxonomy/attribute_set.h>
#include <nx/vms/client/core/analytics/taxonomy/utils.h>

#include "utils.h"

namespace nx::vms::client::core::analytics::taxonomy {

static bool objectTypeMatchesFilter(
    const AbstractStateViewFilter* filter,
    const nx::analytics::taxonomy::AbstractObjectType* objectType)
{
    return (!filter || filter->matches(objectType)) && objectType->hasEverBeenSupported();
}

struct ObjectType::Private
{
    const nx::analytics::taxonomy::AbstractObjectType* const mainObjectType = nullptr;
    std::map<QString, const nx::analytics::taxonomy::AbstractObjectType*> additionalObjectTypes;
    mutable std::optional<std::vector<QString>> cachedTypeIds;
    mutable std::optional<std::vector<QString>> cachedFullSubtreeTypeIds;
    QPointer<ObjectType> baseObjectType;
    std::vector<ObjectType*> derivedObjectTypes;
    std::vector<Attribute*> attributes;
    const AbstractStateViewFilter* filter = nullptr;
};

ObjectType::ObjectType(
    const nx::analytics::taxonomy::AbstractObjectType* mainObjectType,
    QObject* parent)
    :
    QObject(parent),
    d(new Private{.mainObjectType = mainObjectType})
{
    NX_ASSERT(d->mainObjectType);
}

ObjectType::~ObjectType()
{
    // Required here for forward-declared scoped pointer destruction.
}

void ObjectType::addDerivedObjectType(ObjectType* objectType)
{
    if (!NX_ASSERT(objectType))
        return;

    NX_ASSERT(objectType->baseObjectType() == nullptr);

    objectType->setBaseObjectType(this);
    d->derivedObjectTypes.push_back(objectType);
}

void ObjectType::addObjectType(const nx::analytics::taxonomy::AbstractObjectType* objectType)
{
    if (!NX_ASSERT(objectType))
        return;

    d->additionalObjectTypes[objectType->id()] = objectType;
}

std::vector<QString> ObjectType::typeIds() const
{
    if (d->cachedTypeIds)
        return *d->cachedTypeIds;

    d->cachedTypeIds = std::vector<QString>();
    if (objectTypeMatchesFilter(d->filter, d->mainObjectType))
        d->cachedTypeIds->push_back(d->mainObjectType->id());

    for (const auto& [objectTypeId, objectType]: d->additionalObjectTypes)
    {
        if (objectTypeMatchesFilter(d->filter, objectType))
            d->cachedTypeIds->push_back(objectTypeId);
    }

    return *d->cachedTypeIds;
}

QString ObjectType::mainTypeId() const
{
    return d->mainObjectType->id();
}

std::vector<QString> ObjectType::fullSubtreeTypeIds() const
{
    if (d->cachedFullSubtreeTypeIds)
        return *d->cachedFullSubtreeTypeIds;

    std::set<QString> result;
    for (const ObjectType* derivedObjectType: d->derivedObjectTypes)
    {
        const std::vector<QString> derivedObjectTypeSubtreeTypeIds =
            derivedObjectType->fullSubtreeTypeIds();
        result.insert(
            derivedObjectTypeSubtreeTypeIds.begin(),
            derivedObjectTypeSubtreeTypeIds.end());
    }

    std::vector<QString> ownTypeIds = this->typeIds();
    result.insert(ownTypeIds.begin(), ownTypeIds.end());

    d->cachedFullSubtreeTypeIds = std::vector<QString>(result.begin(), result.end());
    return *d->cachedFullSubtreeTypeIds;
}

QString ObjectType::id() const
{
    return d->mainObjectType->id();
}

QString ObjectType::name() const
{
    if (NX_ASSERT(d->mainObjectType))
        return d->mainObjectType->name();

    return QString();
}

QString ObjectType::icon() const
{
    if (NX_ASSERT(d->mainObjectType))
        return d->mainObjectType->icon();

    return QString();
}

std::vector<Attribute*> ObjectType::attributes() const
{
    return d->attributes;
}

ObjectType* ObjectType::baseObjectType() const
{
    return d->baseObjectType;
}

std::vector<ObjectType*> ObjectType::derivedObjectTypes() const
{
    return d->derivedObjectTypes;
}

void ObjectType::setFilter(const AbstractStateViewFilter* filter)
{
    d->filter = filter;
}

void ObjectType::resolveAttributes()
{
    std::vector<const nx::analytics::taxonomy::AbstractObjectType*> objectTypes;

    if (objectTypeMatchesFilter(d->filter, d->mainObjectType))
        objectTypes.push_back(d->mainObjectType);

    for (const auto& [_, objectType]: d->additionalObjectTypes)
    {
        if (objectTypeMatchesFilter(d->filter, objectType))
            objectTypes.push_back(objectType);
    }

    d->attributes = taxonomy::resolveAttributes(objectTypes, d->filter, this);
}

QString ObjectType::makeId(const QStringList& analyticsObjectTypeIds)
{
    return analyticsObjectTypeIds.join("|");
}

void ObjectType::setBaseObjectType(ObjectType* objectType)
{
    d->baseObjectType = objectType;
}

} // namespace nx::vms::client::core::analytics::taxonomy
