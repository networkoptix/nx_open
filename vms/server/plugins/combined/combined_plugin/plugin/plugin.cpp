// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <nx/vms_server_plugins/analytics/sample/integration.h>
#include <nx/vms_server_plugins/cloud_storage/sample/integration.h>
#include <image_library_plugin.h>
#include <test_storage_factory.h>

extern "C" NX_PLUGIN_API nx::sdk::IIntegration* createNxPluginByIndex(int instanceIndex)
{
    switch (instanceIndex)
    {
        case 0: return new nx::vms_server_plugins::analytics::sample::Integration();
        case 1: return new nx::vms_server_plugins::cloud_storage::sample::Integration();
        case 2: return reinterpret_cast<nx::sdk::IIntegration*>(new ImageLibraryPlugin());
        case 3: return reinterpret_cast<nx::sdk::IIntegration*>(new TestStorageFactory());
        default: return nullptr;
    }
}
