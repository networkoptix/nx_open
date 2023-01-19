// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "plugin.h"

#include <nx/kit/debug.h>

#include <nx/sdk/helpers/string.h>
#include <nx/sdk/helpers/error.h>
#include <nx/sdk/helpers/lib_context.h>

#include "engine.h"

namespace nx::sdk::analytics {

Plugin::Plugin()
{
    logCreation();
}

Plugin::Plugin(std::string pluginManifest, CreateEngine createEngine):
    m_jsonManifest(std::move(pluginManifest)), m_createEngineFunc(std::move(createEngine))
{
    logCreation();
}

Plugin::~Plugin()
{
    logDestruction();
}

std::string Plugin::manifestString() const
{
    NX_KIT_ASSERT(false,
        "Either manifestString() should be overridden, or the constructor with pluginManifest "
        "argument (deprecated) should be called.");
    return "";
}

Result<IEngine*> Plugin::doObtainEngine()
{
    static const char errorMessage[] = "Either doObtainEngine() should be overridden, or the "
        "constructor with createEngine argument (deprecated) should be called.";
    NX_KIT_ASSERT(false, errorMessage);
    return error(ErrorCode::internalError, errorMessage);
}

void Plugin::setUtilityProvider(IUtilityProvider* utilityProvider)
{
    m_utilityProvider = shareToPtr(utilityProvider);
}

void Plugin::getManifest(Result<const IString*>* outResult) const
{
    *outResult = new String(m_jsonManifest.empty() ? manifestString() : m_jsonManifest);
}

void Plugin::doCreateEngine(Result<IEngine*>* outResult)
{
    if (!m_createEngineFunc)
    {
        *outResult = doObtainEngine();
        if (!outResult->isOk())
            logError(outResult->error().errorMessage()->str());
    }
    else
    {
        *outResult = m_createEngineFunc(this);
        if (outResult->value())
            return;

        const std::string errorMessage = "Unable to create Engine";
        logError(errorMessage);
        *outResult = error(ErrorCode::otherError, errorMessage);
    }
}

void Plugin::logLifeCycleEvent(const std::string& event) const
{
    NX_PRINT << event << " IPlugin @" << nx::kit::utils::toString(this)
        << " of " << libContext().name();
}

void Plugin::logError(const std::string& message) const
{
    NX_PRINT << "ERROR: " << libContext().name() << ": " << message;
}

} // namespace nx::sdk::analytics
