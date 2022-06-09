// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "engine_state_view_filter.h"

#include <nx/analytics/taxonomy/abstract_engine.h>
#include <nx/analytics/taxonomy/abstract_object_type.h>
#include <nx/analytics/taxonomy/abstract_scope.h>

namespace nx::vms::client::desktop::analytics::taxonomy {

struct EngineStateViewFilter::Private
{
    const nx::analytics::taxonomy::AbstractEngine* const engine = nullptr;
};

EngineStateViewFilter::EngineStateViewFilter(
    const nx::analytics::taxonomy::AbstractEngine* engine,
    QObject* parent)
    :
    AbstractStateViewFilter(parent),
    d(new Private{engine})
{
}

EngineStateViewFilter::~EngineStateViewFilter()
{
    // Required here for forward-declared scoped pointer destruction.
}

QString EngineStateViewFilter::id() const
{
    if (d->engine)
        return d->engine->id();

    return QString();
}

QString EngineStateViewFilter::name() const
{
    if (d->engine)
        return d->engine->name();

    return QString();
}

bool EngineStateViewFilter::matches(
    const nx::analytics::taxonomy::AbstractObjectType* objectType) const
{
    if (!d->engine)
        return true;

    for (const nx::analytics::taxonomy::AbstractScope* scope: objectType->scopes())
    {
        const nx::analytics::taxonomy::AbstractEngine* engine = scope->engine();
        if (engine && d->engine->id() == engine->id())
            return true;
    }

    return false;
}

bool EngineStateViewFilter::matches(
    const nx::analytics::taxonomy::AbstractAttribute* attribute) const
{
    // TODO: implement this when Taxonomy supports attributeSupportInfo.
    return true;
}

} // namespace nx::vms::client::desktop::analytics::taxonomy
