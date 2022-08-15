// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "state_view_builder.h"

#include "node.h"
#include "state_view.h"
#include "engine_state_view_filter.h"

namespace nx::vms::client::desktop::analytics::taxonomy {

static void engineFiltersFromObjectType(
    const nx::analytics::taxonomy::AbstractObjectType* objectType,
    std::map<QString, AbstractStateViewFilter*>* inOutEngineFilterById,
    QObject* parent)
{
    if (!NX_ASSERT(inOutEngineFilterById))
        return;

    for (const nx::analytics::taxonomy::AbstractScope* scope: objectType->scopes())
    {
        // Skip the Type which is in the Engine scope but has never been supported by any Device Agent.
        if (scope->deviceIds().empty())
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

static AbstractNode* makeFilteredNode(
    const nx::analytics::taxonomy::AbstractObjectType* objectType,
    const AbstractStateViewFilter* filter,
    QObject* parent,
    std::map<QString, AbstractStateViewFilter*>* outEngineFilters = nullptr)
{
    if (!objectType->isReachable())
        return nullptr;

    std::vector<const nx::analytics::taxonomy::AbstractObjectType*> nodeObjectTypes;
    std::vector<AbstractNode*> derivedNodes;

    for (nx::analytics::taxonomy::AbstractObjectType* derivedObjectType:
        objectType->derivedTypes())
    {
        if (isHidden(derivedObjectType) && (!filter || filter->matches(derivedObjectType)))
        {
            nodeObjectTypes.push_back(derivedObjectType);

            if (outEngineFilters)
                engineFiltersFromObjectType(derivedObjectType, outEngineFilters, parent);
        }
        else if (derivedObjectType->isReachable())
        {
            if (AbstractNode* node = makeFilteredNode(derivedObjectType, filter, parent))
                derivedNodes.push_back(node);
        }
    }

    const bool directFilterMatch = !filter || filter->matches(objectType);
    if (nodeObjectTypes.empty() && derivedNodes.empty() && !directFilterMatch)
        return nullptr;

    if (directFilterMatch && outEngineFilters)
        engineFiltersFromObjectType(objectType, outEngineFilters, parent);

    Node* node = new Node(objectType, parent);
    node->setFilter(filter);
    for (const nx::analytics::taxonomy::AbstractObjectType* nodeObjectType: nodeObjectTypes)
        node->addObjectType(nodeObjectType);

    for (AbstractNode* derivedNode: derivedNodes)
        node->addDerivedNode(derivedNode);

    node->resolveAttributes();
    return node;
}

static AbstractStateView* makeFilteredView(
    const std::shared_ptr<nx::analytics::taxonomy::AbstractState>& taxonomyState,
    const AbstractStateViewFilter* filter,
    std::map<QString, AbstractStateViewFilter*>* outEngineFilters = nullptr)
{
    const std::vector<nx::analytics::taxonomy::AbstractObjectType*> rootObjectTypes =
        taxonomyState->rootObjectTypes();

    std::vector<AbstractNode*> result;
    for (nx::analytics::taxonomy::AbstractObjectType* objectType: rootObjectTypes)
    {
        if (AbstractNode* node =
            makeFilteredNode(objectType, filter, taxonomyState.get(), outEngineFilters))
        {
            result.push_back(node);
        }
    }

    return new StateView(std::move(result), taxonomyState.get());
}

struct StateViewBuilder::Private
{
    std::shared_ptr<nx::analytics::taxonomy::AbstractState> taxonomyState;
    std::vector<AbstractStateViewFilter*> engineFilters;
    mutable std::map<QString, AbstractStateView*> cachedStateViews;
};

StateViewBuilder::StateViewBuilder(
    std::shared_ptr<nx::analytics::taxonomy::AbstractState> taxonomyState):
    d(new Private{std::move(taxonomyState)})
{
    std::map<QString, AbstractStateViewFilter*> engineFilters;
    AbstractStateView* defaultStateView = makeFilteredView(d->taxonomyState, nullptr, &engineFilters);

    d->cachedStateViews[QString()] = defaultStateView;
    for (const auto& [_, engineFilter]: engineFilters)
        d->engineFilters.push_back(engineFilter);
}

StateViewBuilder::~StateViewBuilder()
{
    // Required here for forward-declared scoped pointer destruction.
}

AbstractStateView* StateViewBuilder::stateView(const AbstractStateViewFilter* filter) const
{
    const QString filterId = filter ? filter->id() : QString();
    const auto itr = d->cachedStateViews.find(filterId);
    if (itr != d->cachedStateViews.cend())
        return itr->second;

    AbstractStateView* view = makeFilteredView(d->taxonomyState, filter);
    d->cachedStateViews[filterId] = view;

    return view;
}

std::vector<AbstractStateViewFilter*> StateViewBuilder::engineFilters() const
{
    return d->engineFilters;
}

} // namespace nx::vms::client::desktop::analytics::taxonomy
