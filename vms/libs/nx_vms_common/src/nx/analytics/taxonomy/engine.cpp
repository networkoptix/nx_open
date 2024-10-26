// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <nx/analytics/taxonomy/engine.h>
#include <nx/analytics/taxonomy/error_handler.h>
#include <nx/analytics/taxonomy/integration.h>
#include <nx/analytics/taxonomy/internal_state.h>
#include <nx/kit/utils.h>

using namespace nx::vms::api::analytics;

namespace nx::analytics::taxonomy {

Engine::Engine(EngineDescriptor engineDescriptor, QObject* parent):
    AbstractEngine(parent),
    m_descriptor(std::move(engineDescriptor))
{
}

QString Engine::id() const
{
    return m_descriptor.id.toString();
}

QString Engine::name() const
{
    return m_descriptor.name;
}

AbstractIntegration* Engine::integration() const
{
    return m_integration;
}

nx::vms::api::analytics::EngineCapabilities Engine::capabilities() const
{
    return m_descriptor.capabilities;
}

void Engine::resolve(InternalState* inOutInternalState, ErrorHandler* errorHandler)
{
    m_integration = inOutInternalState->getTypeById<Integration>(m_descriptor.pluginId);
    if (!m_integration)
    {
        errorHandler->handleError(
            ProcessingError{NX_FMT(
                "Engine %1: unable to find parent Integration %2",
                m_descriptor.id,
                nx::kit::utils::toString(m_descriptor.pluginId)
            )});
    }
}

EngineDescriptor Engine::serialize() const
{
    return m_descriptor;
}

} // namespace nx::analytics::taxonomy
