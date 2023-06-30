// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "scope_state_view_filter.h"

#include <nx/analytics/taxonomy/abstract_engine.h>
#include <nx/analytics/taxonomy/abstract_object_type.h>
#include <nx/analytics/taxonomy/abstract_scope.h>

namespace nx::vms::client::core::analytics::taxonomy {

struct ScopeStateViewFilter::Private
{
    nx::analytics::taxonomy::AbstractEngine* engine = nullptr;
    std::set<QnUuid> devices;
    QString id;
};

ScopeStateViewFilter::ScopeStateViewFilter(
    nx::analytics::taxonomy::AbstractEngine* engine,
    const std::set<QnUuid>& devices,
    QObject* parent)
    :
    AbstractStateViewFilter(parent),
    d(new Private{engine, devices})
{
    QStringList ids = {engine ? engine->id() : ""};

    for (const QnUuid& id: devices)
        ids.push_back(id.toString());

    d->id = ids.join(" ");
}

ScopeStateViewFilter::~ScopeStateViewFilter()
{
    // Required here for forward-declared scoped pointer destruction.
}

QString ScopeStateViewFilter::id() const
{
    return d->id;
}

QString ScopeStateViewFilter::name() const
{
    return d->id;
}

bool ScopeStateViewFilter::matches(
    const nx::analytics::taxonomy::AbstractObjectType* objectType) const
{
    if (d->devices.empty())
        return false;

    if (!objectType->hasEverBeenSupported())
        return false;


    const QnUuid engineId = d->engine ? QnUuid::fromStringSafe(d->engine->id()) : QnUuid();
    for (const QnUuid& deviceId: d->devices)
    {
        if (objectType->isSupported(engineId, deviceId))
            return true;
    }

    return false;
}

bool ScopeStateViewFilter::matches(
    const nx::analytics::taxonomy::AbstractAttribute* attribute) const
{
    const auto engine = d->engine ? QnUuid{d->engine->id()} : QnUuid{};

    if (d->devices.empty())
        return attribute->isSupported(engine, /*deviceId*/ {});

    return std::any_of(d->devices.begin(), d->devices.end(),
        [&](const QnUuid& device) { return attribute->isSupported(engine, device); });
}

} // namespace nx::vms::client::core::analytics::taxonomy
