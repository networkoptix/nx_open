// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "plugin.h"

#include <nx/kit/debug.h>

#include <nx/sdk/helpers/string.h>
#include <nx/sdk/helpers/error.h>

#include "engine.h"

namespace nx {
namespace sdk {
namespace analytics {

Plugin::Plugin(
    std::string pluginName,
    std::string pluginManifest,
    CreateEngine createEngine)
    :
    m_name(std::move(pluginName)),
    m_jsonManifest(std::move(pluginManifest)),
    m_createEngine(std::move(createEngine))
{
    NX_PRINT << "Created " << m_name << "[" << this << "]";
}

Plugin::~Plugin()
{
    NX_PRINT << "Destroyed " << m_name << "[" << this << "]";
}

const char* Plugin::name() const
{
    return m_name.c_str();
}

void Plugin::setUtilityProvider(IUtilityProvider* utilityProvider)
{
    utilityProvider->addRef();
    m_utilityProvider.reset(utilityProvider);
}

Result<const IString*> Plugin::manifest() const
{
    return new String(m_jsonManifest);
}

Result<IEngine*> Plugin::createEngine()
{
    IEngine* engine = m_createEngine(this);
    if (!engine)
    {
        const std::string errorMessage{"createEngine() failed"};
        NX_PRINT << "ERROR: " << m_name << ": " << errorMessage;
        return error(ErrorCode::internalError, errorMessage);
    }
    return engine;
}

} // namespace analytics
} // namespace sdk
} // namespace nx
