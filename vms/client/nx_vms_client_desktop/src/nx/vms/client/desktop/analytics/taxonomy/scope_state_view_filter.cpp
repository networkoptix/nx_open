// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "scope_state_view_filter.h"

#include <nx/analytics/taxonomy/abstract_engine.h>
#include <nx/analytics/taxonomy/abstract_object_type.h>
#include <nx/analytics/taxonomy/abstract_scope.h>

namespace nx::vms::client::desktop::analytics::taxonomy {

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

    const auto scopes = objectType->scopes();

    return std::any_of(scopes.begin(), scopes.end(),
        [&](const nx::analytics::taxonomy::AbstractScope* scope)
        {
            const nx::analytics::taxonomy::AbstractEngine* engine = scope->engine();
            const bool isEngineMatched = !d->engine || (d->engine->id() == engine->id());

            const auto scopeDeviceList = scope->deviceIds();
            const std::set<QnUuid> scopeDevices = {scopeDeviceList.begin(), scopeDeviceList.end()};

            std::vector<QnUuid> matches;
            std::set_intersection(
                scopeDevices.begin(), scopeDevices.end(),
                d->devices.begin(), d->devices.end(),
                std::back_inserter(matches));

            const bool isDevicesMatched = !matches.empty();

            return isDevicesMatched && isEngineMatched;
        });
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

} // namespace nx::vms::client::desktop::analytics::taxonomy
