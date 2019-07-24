// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "plugin.h"

#include <nx/kit/debug.h>

#include <nx/sdk/helpers/string.h>
#include <nx/sdk/helpers/error.h>
#include <nx/sdk/helpers/lib_context.h>

#include "engine.h"

namespace nx {
namespace sdk {
namespace analytics {

Plugin::Plugin(std::string pluginManifest, CreateEngine createEngine):
    m_jsonManifest(std::move(pluginManifest)), m_createEngine(std::move(createEngine))
{
    NX_PRINT << "Created " << libContext().name() << "[" << this << "]";
}

Plugin::~Plugin()
{
    NX_PRINT << "Destroyed " << libContext().name() << "[" << this << "]";
}

void Plugin::setUtilityProvider(IUtilityProvider* utilityProvider)
{
    utilityProvider->addRef();
    m_utilityProvider.reset(utilityProvider);
}

StringResult Plugin::manifest() const
{
    return new String(m_jsonManifest);
}

MutableEngineResult Plugin::createEngine()
{
    IEngine* engine = m_createEngine(this);
    if (!engine)
    {
        const std::string errorMessage = "Unable to create Engine";
        NX_PRINT << "ERROR: " << libContext().name() << ": " << errorMessage;
        return error(ErrorCode::otherError, errorMessage);
    }
    return engine;
}

} // namespace analytics
} // namespace sdk
} // namespace nx
