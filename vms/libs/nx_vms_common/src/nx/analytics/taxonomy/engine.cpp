// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <nx/analytics/taxonomy/engine.h>
#include <nx/analytics/taxonomy/plugin.h>

#include <nx/analytics/taxonomy/internal_state.h>
#include <nx/analytics/taxonomy/error_handler.h>

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

AbstractPlugin* Engine::plugin() const
{
    return m_plugin;
}

nx::vms::api::analytics::EngineCapabilities Engine::capabilities() const
{
    return m_descriptor.capabilities;
}

void Engine::setPlugin(Plugin* plugin)
{
    m_plugin = plugin;
}

void Engine::resolve(InternalState* inOutInternalState, ErrorHandler* errorHandler)
{
    m_plugin = inOutInternalState->getTypeById<Plugin>(m_descriptor.pluginId);
    if (!m_plugin)
    {
        errorHandler->handleError(
            ProcessingError{NX_FMT(
                "Engine %1: unable to find parent Plugin (%2)",
                m_descriptor.id, m_descriptor.pluginId)});
    }
}

EngineDescriptor Engine::serialize() const
{
    return m_descriptor;
}

} // namespace nx::analytics::taxonomy
