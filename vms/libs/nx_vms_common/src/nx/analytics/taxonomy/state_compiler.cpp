// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "state_compiler.h"

#include <nx/vms/api/analytics/manifest_items.h>

#include <nx/analytics/taxonomy/enum_type.h>
#include <nx/analytics/taxonomy/color_type.h>
#include <nx/analytics/taxonomy/object_type.h>
#include <nx/analytics/taxonomy/event_type.h>
#include <nx/analytics/taxonomy/engine.h>
#include <nx/analytics/taxonomy/integration.h>
#include <nx/analytics/taxonomy/group.h>
#include <nx/analytics/taxonomy/primary_resolver.h>
#include <nx/analytics/taxonomy/error_handler.h>
#include <nx/analytics/taxonomy/internal_state.h>
#include <nx/analytics/taxonomy/state.h>
#include <nx/analytics/taxonomy/resource_support_proxy.h>

namespace nx::analytics::taxonomy {

using namespace nx::vms::api::analytics;

template<typename Map>
void resolveMap(Map* inOutMap, InternalState* internalState, ErrorHandler* errorHandler)
{
    for (auto& [_, item]: *inOutMap)
        item->resolve(internalState, errorHandler);
}

template<typename Map>
void resolveSupportedAttributes(
    Map* inOutMap, InternalState* internalState, ErrorHandler* errorHandler)
{
    for (auto& [_, item] : *inOutMap)
        item->resolveSupportedAttributes(internalState, errorHandler);
}

template<typename Map>
void resolveReachability(Map* inOutMap)
{
    for (auto& [_, item]: *inOutMap)
        item->resolveReachability();
}

static InternalState makeInitialInternalState(
    Descriptors descriptors,
    AbstractResourceSupportProxy* resourceSupportProxy)
{
    InternalState internalState;

    for (auto& [id, descriptor]: descriptors.pluginDescriptors)
        internalState.integrationById[id] = new Integration(std::move(descriptor));

    for (auto& [id, descriptor]: descriptors.engineDescriptors)
        internalState.engineById[id.toString()] = new Engine(std::move(descriptor));

    for (auto& [id, descriptor]: descriptors.groupDescriptors)
        internalState.groupById[id] = new Group(std::move(descriptor));

    for (auto& [id, descriptor]: descriptors.enumTypeDescriptors)
        internalState.enumTypeById[id] = new EnumType(std::move(descriptor));

    for (auto& [id, descriptor]: descriptors.colorTypeDescriptors)
        internalState.colorTypeById[id] = new ColorType(std::move(descriptor));

    for (auto& [id, descriptor]: descriptors.eventTypeDescriptors)
    {
        internalState.eventTypeById[id] =
            new EventType(std::move(descriptor), resourceSupportProxy);
    }

    for (auto& [id, descriptor]: descriptors.objectTypeDescriptors)
    {
        internalState.objectTypeById[id] =
            new ObjectType(std::move(descriptor), resourceSupportProxy);
    }

    for (auto& [id, descriptor]: descriptors.attributeListDescriptors)
    {
        for (AttributeDescription& attributeDescription: descriptor.attributes)
            internalState.attributeListById[id].push_back(std::move(attributeDescription));
    }

    return internalState;
}

/*static*/
StateCompiler::Result StateCompiler::compile(
    Descriptors descriptors,
    std::unique_ptr<AbstractResourceSupportProxy> resourceSupportProxy)
{
    ErrorHandler errorHandler;

    PrimaryResolver::resolve(&descriptors, &errorHandler);

    InternalState internalState =
        makeInitialInternalState(std::move(descriptors), resourceSupportProxy.get());

    resolveMap(&internalState.engineById, &internalState, &errorHandler);
    resolveMap(&internalState.enumTypeById, &internalState, &errorHandler);
    resolveMap(&internalState.colorTypeById, &internalState, &errorHandler);
    resolveMap(&internalState.objectTypeById, &internalState, &errorHandler);
    resolveMap(&internalState.eventTypeById, &internalState, &errorHandler);

    resolveSupportedAttributes(&internalState.objectTypeById, &internalState, &errorHandler);
    resolveSupportedAttributes(&internalState.eventTypeById, &internalState, &errorHandler);
    resolveReachability(&internalState.objectTypeById);
    resolveReachability(&internalState.eventTypeById);

    StateCompiler::Result result;
    result.state = std::make_shared<State>(
        std::move(internalState),
        std::move(resourceSupportProxy));

    result.errors = errorHandler.errors();

    return result;
}

} // namespace nx::analytics::taxonomy
