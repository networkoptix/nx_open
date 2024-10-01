// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "integration.h"

#include <nx/kit/debug.h>

#include <nx/sdk/helpers/string.h>
#include <nx/sdk/helpers/error.h>
#include <nx/sdk/helpers/lib_context.h>

#include "engine.h"

namespace nx::sdk::analytics {

Integration::Integration()
{
    logCreation();
}

Integration::Integration(std::string integrationManifest, CreateEngine createEngine):
    m_jsonManifest(std::move(integrationManifest)), m_createEngineFunc(std::move(createEngine))
{
    logCreation();
}

Integration::~Integration()
{
    logDestruction();
}

std::string Integration::manifestString() const
{
    NX_KIT_ASSERT(false,
        "Either manifestString() should be overridden, or the constructor with the "
        "integrationManifest argument (deprecated) should be called.");
    return "";
}

Result<IEngine*> Integration::doObtainEngine()
{
    static const char errorMessage[] = "Either doObtainEngine() should be overridden, or the "
        "constructor with createEngine argument (deprecated) should be called.";
    NX_KIT_ASSERT(false, errorMessage);
    return error(ErrorCode::internalError, errorMessage);
}

void Integration::setUtilityProvider(IUtilityProvider* utilityProvider)
{
    m_utilityProvider = shareToPtr(utilityProvider);
}

void Integration::getManifest(Result<const IString*>* outResult) const
{
    *outResult = new String(m_jsonManifest.empty() ? manifestString() : m_jsonManifest);
}

void Integration::doCreateEngine(Result<IEngine*>* outResult)
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

void Integration::logLifeCycleEvent(const std::string& event) const
{
    NX_PRINT << event << " IIntegration @" << nx::kit::utils::toString(this)
        << " of " << libContext().name();
}

void Integration::logError(const std::string& message) const
{
    NX_PRINT << "ERROR: " << libContext().name() << ": " << message;
}

} // namespace nx::sdk::analytics
