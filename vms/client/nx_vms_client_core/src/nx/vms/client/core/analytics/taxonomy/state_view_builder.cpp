// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "state_view_builder.h"

#include "engine_state_view_filter.h"
#include "object_type.h"
#include "state_view.h"

namespace nx::vms::client::core::analytics::taxonomy {

static void engineFiltersFromObjectType(
    const nx::analytics::taxonomy::AbstractObjectType* objectType,
    std::map<QString, AbstractStateViewFilter*>* inOutEngineFilterById,
    QObject* parent)
{
    if (!NX_ASSERT(inOutEngineFilterById))
        return;

    if (!objectType->hasEverBeenSupported())
        return;

    for (const nx::analytics::taxonomy::AbstractScope* scope: objectType->scopes())
    {
        // Skip the Type which is in the Engine scope but has never been supported by any Device Agent.
        if (!scope->hasTypeEverBeenSupportedInThisScope())
            continue;

        nx::analytics::taxonomy::AbstractEngine* taxonomyEngine = scope->engine();
        if (!taxonomyEngine)
            continue;

        const QString engineId = taxonomyEngine->id();
        if (inOutEngineFilterById->find(engineId) == inOutEngineFilterById->cend())
        {
            inOutEngineFilterById->emplace(
                engineId,
                new EngineStateViewFilter(taxonomyEngine, parent));
        }
    }
}

static bool isHidden(const nx::analytics::taxonomy::AbstractObjectType* objectType)
{
    return !objectType->isReachable() && objectType->hasEverBeenSupported();
}

static ObjectType* makeFilteredObjectType(
    const nx::analytics::taxonomy::AbstractObjectType* objectType,
    const AbstractStateViewFilter* filter,
    QObject* parent,
    std::map<QString, AbstractStateViewFilter*>* outEngineFilters = nullptr)
{
    if (!objectType->isReachable())
        return nullptr;

    std::vector<const nx::analytics::taxonomy::AbstractObjectType*> filteredObjectTypes;
    std::vector<ObjectType*> derivedObjectTypes;

    for (nx::analytics::taxonomy::AbstractObjectType* derivedObjectType:
        objectType->derivedTypes())
    {
        if (isHidden(derivedObjectType) && (!filter || filter->matches(derivedObjectType)))
        {
            filteredObjectTypes.push_back(derivedObjectType);

            if (outEngineFilters)
                engineFiltersFromObjectType(derivedObjectType, outEngineFilters, parent);
        }
        else if (derivedObjectType->isReachable())
        {
            if (ObjectType* derived = makeFilteredObjectType(derivedObjectType, filter, parent))
                derivedObjectTypes.push_back(derived);
        }
    }

    const bool directFilterMatch = !filter || filter->matches(objectType);
    if (filteredObjectTypes.empty() && derivedObjectTypes.empty() && !directFilterMatch)
        return nullptr;

    if (directFilterMatch && outEngineFilters)
        engineFiltersFromObjectType(objectType, outEngineFilters, parent);

    ObjectType* result = new ObjectType(objectType, nullptr);
    result->moveToThread(parent->thread());
    result->setParent(parent);
    result->setFilter(filter);
    for (const nx::analytics::taxonomy::AbstractObjectType* baseObjectType: filteredObjectTypes)
        result->addObjectType(baseObjectType);

    for (ObjectType* derived: derivedObjectTypes)
        result->addDerivedObjectType(derived);

    result->resolveAttributes();
    return result;
}

static StateView* makeFilteredView(
    const std::shared_ptr<nx::analytics::taxonomy::AbstractState>& taxonomyState,
    const AbstractStateViewFilter* filter,
    std::map<QString, AbstractStateViewFilter*>* outEngineFilters = nullptr,
    QObject* parent = nullptr)
{
    const std::vector<nx::analytics::taxonomy::AbstractObjectType*> rootObjectTypes =
        taxonomyState->rootObjectTypes();

    std::vector<ObjectType*> result;
    for (nx::analytics::taxonomy::AbstractObjectType* objectType: rootObjectTypes)
    {
        if (ObjectType* filteredObjectType =
            makeFilteredObjectType(objectType, filter, taxonomyState.get(), outEngineFilters))
        {
            result.push_back(filteredObjectType);
        }
    }

    return new StateView(std::move(result), parent);
}

struct StateViewBuilder::Private
{
    std::shared_ptr<nx::analytics::taxonomy::AbstractState> taxonomyState;
    std::vector<AbstractStateViewFilter*> engineFilters;
    mutable std::map<QString, StateView*> cachedStateViews;
};

StateViewBuilder::StateViewBuilder(
    std::shared_ptr<nx::analytics::taxonomy::AbstractState> taxonomyState):
    d(new Private{std::move(taxonomyState)})
{
    std::map<QString, AbstractStateViewFilter*> engineFilters;
    StateView* defaultStateView = makeFilteredView(
        d->taxonomyState,
        /*filter*/ nullptr,
        /*outEngineFilters*/ &engineFilters,
        /*parent*/ d->taxonomyState.get());

    d->cachedStateViews[QString()] = defaultStateView;
    for (const auto& [_, engineFilter]: engineFilters)
        d->engineFilters.push_back(engineFilter);
}

StateViewBuilder::~StateViewBuilder()
{
    // Required here for forward-declared scoped pointer destruction.
}

StateView* StateViewBuilder::stateView(const AbstractStateViewFilter* filter) const
{
    const QString filterId = filter ? filter->id() : QString();
    const auto itr = d->cachedStateViews.find(filterId);
    if (itr != d->cachedStateViews.cend())
        return itr->second;

    StateView* view = stateView(d->taxonomyState, filter, /*parent*/ d->taxonomyState.get());
    d->cachedStateViews[filterId] = view;

    return view;
}

StateView* StateViewBuilder::stateView(
    std::shared_ptr<nx::analytics::taxonomy::AbstractState> taxonomyState,
    const AbstractStateViewFilter* filter,
    QObject* parent)
{
    return makeFilteredView(taxonomyState, filter, /*outEngineFilters*/ nullptr, parent);
}

std::vector<AbstractStateViewFilter*> StateViewBuilder::engineFilters() const
{
    return d->engineFilters;
}

} // namespace nx::vms::client::core::analytics::taxonomy
