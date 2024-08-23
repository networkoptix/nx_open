// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "plugin.h"

#include <nx/sdk/helpers/string.h>
#include <nx/sdk/cloud_storage/helpers/data.h>
#include <nx/kit/json.h>

namespace nx::vms_server_plugins::cloud_storage::sample {

static const nx::sdk::cloud_storage::PluginManifest kPluginManifest = {
    /* id */ "samplecloud",
    /* name */ "Sample cloud storage plugin",
    /* description */ "Basic compilable implementation of the cloud storage plugin",
    /* version */ "1.0.0",
    /* vendor */ "Network Optix"
};

Plugin::~Plugin()
{
}

void Plugin::doObtainEngine(
    const char* url,
    const nx::sdk::cloud_storage::IArchiveUpdateHandler* /*archiveUpdateHandler*/,
    nx::sdk::Result<nx::sdk::cloud_storage::IEngine*>* outResult)
{
    *outResult = nx::sdk::Result<nx::sdk::cloud_storage::IEngine*>(
        nx::sdk::Error(nx::sdk::ErrorCode::notImplemented, new nx::sdk::String("Not implemented")));
}

void Plugin::setUtilityProvider(nx::sdk::IUtilityProvider* /*utilityProvider*/)
{
}

void Plugin::getManifest(nx::sdk::Result<const nx::sdk::IString*>* outResult) const
{
    *outResult = nx::sdk::Result<const nx::sdk::IString*>(
        new nx::sdk::String(nx::kit::Json(kPluginManifest).dump()));
}

} // nx::vms_server_plugins::cloud_storage::sample

extern "C" NX_PLUGIN_API nx::sdk::IPlugin* createNxPlugin()
{
    return new nx::vms_server_plugins::cloud_storage::sample::Plugin;
}
