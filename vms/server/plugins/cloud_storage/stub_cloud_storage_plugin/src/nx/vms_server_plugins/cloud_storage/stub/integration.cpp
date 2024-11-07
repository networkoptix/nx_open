// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <iostream>
#include <string>

#include <nx/kit/debug.h>
#include <nx/sdk/helpers/string.h>

#include "integration.h"
#include "engine.h"
#include "stub_cloud_storage_plugin_ini.h"

namespace nx::vms_server_plugins::cloud_storage::stub {

static const nx::sdk::cloud_storage::IntegrationManifest kIntegrationManifest = {
    /* id */ "stubcloud",
    /* name */ "Stub cloud storage plugin",
    /* description */ "Full featured implementation of the cloud storage plugin with the "
                      "memory/local files based backend.",
    /* version */ "1.0.0",
    /* vendor */ "Network Optix"
};

Integration::~Integration()
{
}

void Integration::doObtainEngine(
    const char* url,
    const nx::sdk::cloud_storage::IAsyncOperationHandler* asyncOperationHandler,
    nx::sdk::Result<nx::sdk::cloud_storage::IEngine*>* outResult)
{
    try
    {
        std::lock_guard lock(m_mutex);
        if (!m_engine)
        {
            NX_OUTPUT << __func__ << ": called with url '" << url << "'";

            // Here we may want to create all kinds of data managing clients, connections to
            // databases and backends, which are supposed to be persistent throughout the plugin
            // lifetime.

            std::string workDir;
            const auto testOptions = nx::sdk::unitTestOptions();
            auto workDirIt = testOptions.find("stubCloudStoragePlugin_workDir");
            if (workDirIt != testOptions.cend())
                workDir = workDirIt->second;
            else
                workDir = ini().workDir;

            m_dataManager.reset(new DataManager(workDir));
            m_engine.reset(new Engine(
                asyncOperationHandler, m_dataManager, kIntegrationManifest.id));
        }

        m_engine->addRef();
        *outResult =
            nx::sdk::Result<nx::sdk::cloud_storage::IEngine*>(m_engine.get());
    }
    catch (const std::exception& e)
    {
        *outResult = nx::sdk::Result<nx::sdk::cloud_storage::IEngine*>(
            nx::sdk::Error(nx::sdk::ErrorCode::internalError, new nx::sdk::String(e.what())));
    }
}

/**
 * Called by the Server before all others, so we may safely use m_utilityProvider in other functions.
 */
void Integration::setUtilityProvider(nx::sdk::IUtilityProvider* utilityProvider)
{
    m_utilityProvider.reset(utilityProvider);
    if (m_utilityProvider)
        m_utilityProvider->addRef();
}

void Integration::getManifest(nx::sdk::Result<const nx::sdk::IString*>* outResult) const
{
    *outResult = nx::sdk::Result<const nx::sdk::IString*>(
        new nx::sdk::String(nx::kit::Json(kIntegrationManifest).dump()));
}

} // nx::vms_server_plugins::cloud_storage::stub

extern "C" NX_PLUGIN_API nx::sdk::IIntegration* createNxPlugin()
{
    return new nx::vms_server_plugins::cloud_storage::stub::Integration;
}
