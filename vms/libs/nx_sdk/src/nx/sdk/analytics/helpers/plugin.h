// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>
#include <functional>

#include <nx/sdk/i_utility_provider.h>
#include <nx/sdk/analytics/i_plugin.h>
#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/helpers/ptr.h>
#include <nx/sdk/helpers/result_aliases.h>
#include <nx/sdk/analytics/helpers/result_aliases.h>

#include "engine.h"

namespace nx {
namespace sdk {
namespace analytics {

/**
 * Base class for a typical implementation of an Analytics Plugin. Hides many technical details of
 * the Analytics Plugin SDK, but may limit plugin capabilities - use only when suitable.
 */
class Plugin: public RefCountable<IPlugin>
{
public:
    using CreateEngine = std::function<IEngine*(Plugin* plugin)>;

    /**
     * @param libName Name of the plugin library. It's needed for the logging.
     * @param pluginManifest Plugin manifest to be returned from the manifest method.
     * @param createEngine Functor for engine creation.
     */
    Plugin(
        std::string libName,
        std::string pluginManifest,
        CreateEngine createEngine);

    virtual ~Plugin() override;

    Ptr<IUtilityProvider> utilityProvider() const { return m_utilityProvider; }

//-------------------------------------------------------------------------------------------------
// Not intended to be used by a descendant.

public:
    virtual const char* name() const override;
    virtual void setUtilityProvider(IUtilityProvider* utilityProvider) override;
    virtual StringResult manifest() const override;
    virtual MutableEngineResult createEngine() override;

private:
    const std::string m_name;
    const std::string m_jsonManifest;

    CreateEngine m_createEngine;
    Ptr<IUtilityProvider> m_utilityProvider;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
