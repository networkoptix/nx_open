// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
#include <string>

#include <nx/sdk/analytics/i_integration.h>
#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/i_utility_provider.h>
#include <nx/sdk/ptr.h>

#include "engine.h"

namespace nx::sdk::analytics {

/**
 * Base class for a typical implementation of an Analytics Integration. Hides many technical
 * details of the SDK, but may limit plugin capabilities - use only when suitable.
 */
class Integration: public RefCountable<IIntegration>
{
public:
    using CreateEngine = std::function<IEngine*(Integration* integration)>;

    /**
     * Allows to use this class directly without inhering it.
     *
     * @deprecated Instead of using this constructor, create a subclass, override doObtainEngine()
     *     and manifestString().
     *
     * @param integrationManifest Integration manifest to be returned from manifest().
     * @param createEngine Functor for Engine creation.
     */
    Integration(std::string integrationManifest, CreateEngine createEngine);

    virtual ~Integration() override;

    Ptr<IUtilityProvider> utilityProvider() const { return m_utilityProvider; }

    /**
     * Override for multi-IIntegration libraries; provide integrationId specified in the Manifest.
     */
    virtual std::string instanceId() const { return ""; }

protected:
    Integration();

    /**
     * Override this method instead of doCreateEngine().
     */
    virtual Result<IEngine*> doObtainEngine();

    virtual std::string manifestString() const;

//-------------------------------------------------------------------------------------------------
// Not intended to be used by a descendant.

public:
    virtual void setUtilityProvider(IUtilityProvider* utilityProvider) override;

protected:
    virtual void getManifest(Result<const IString*>* outResult) const override;
    virtual void doCreateEngine(Result<IEngine*>* outResult) override;

private:
    void logLifeCycleEvent(const std::string& event) const;

    void logCreation() const { logLifeCycleEvent("Created"); }
    void logDestruction() const { logLifeCycleEvent("Destroyed"); }

    void logError(const std::string& message) const;

private:
    const std::string m_jsonManifest;

    CreateEngine m_createEngineFunc;
    Ptr<IUtilityProvider> m_utilityProvider;
};

} // namespace nx::sdk::analytics
