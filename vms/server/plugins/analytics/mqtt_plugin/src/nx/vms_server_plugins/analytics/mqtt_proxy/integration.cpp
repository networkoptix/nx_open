// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "integration.h"

#include <iostream>

#include "engine.h"
#include "python_app_host.h"

#if defined(__unix__)
    #include <dlfcn.h>
    #include <stdlib.h>
#endif

namespace nx::vms_server_plugins::analytics::mqtt_proxy {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

Result<IEngine*> Integration::doObtainEngine()
{
    return new Engine(PythonAppHost::getInstance());
}

std::string Integration::manifestString() const
{
    PythonAppHost& appHost = PythonAppHost::getInstance();
    const std::optional<std::string> settingsModel = appHost.callPythonFunctionWithRv(
        "settings_model", {});

    return /*suppress newline*/ 1 + (const char*) R"json(
{
    "id": "nx.mqtt.proxy",
    "name": "Mqtt Proxy",
    "description": "Mqtt proxy plugin.",
    "version": "1.0.0",
    "vendor": "Network Optix",
    "engineSettingsModel": )json" + (settingsModel ? settingsModel.value() : "{}") + R"json(
}
)json";
}

extern "C" NX_PLUGIN_API nx::sdk::IIntegration* createNxPlugin()
{
    if (!PythonAppHost::preparePythonEnvironment())
        return nullptr; //< The error is already logged to stderr.

    std::thread pythonServerThread(PythonAppHost::pythonServer);
    pythonServerThread.detach();

    return new Integration();
}

} // nx::vms_server_plugins::analytics::mqtt_proxy
