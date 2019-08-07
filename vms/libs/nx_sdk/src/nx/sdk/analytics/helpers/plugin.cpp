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

void Plugin::getManifest(Result<const IString*>* outResult) const
{
    *outResult = new String(m_jsonManifest);
}

void Plugin::doCreateEngine(Result<IEngine*>* outResult)
{
    if (IEngine* engine = m_createEngine(this))
    {
        *outResult = engine;
        return;
    }

    const std::string errorMessage = "Unable to create Engine";
    NX_PRINT << "ERROR: " << libContext().name() << ": " << errorMessage;
    *outResult = error(ErrorCode::otherError, errorMessage);
}

} // namespace analytics
} // namespace sdk
} // namespace nx
