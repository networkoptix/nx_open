// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "plugin.h"

#include "engine.h"

namespace nx::vms_server_plugins::analytics::gpt4vision {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

Result<IEngine*> Plugin::doObtainEngine()
{
    return new Engine();
}

std::string Plugin::manifestString() const
{
    return /*suppress newline*/ 1 + (const char*) R"json(
{
    "id": "nx.gpt4vision",
    "name": "GPT4 Analytics Plugin",
    "description": "Analyze video frames using GPT4 Vision API.",
    "version": "1.0.0",
    "vendor": "Plugin vendor",
    "engineSettingsModel": {
        "type": "Settings",
        "items": [
            {
                "type": "GroupBox",
                "caption": "OpenAI.com Access",
                "items": [
                    {
                        "type": "PasswordField",
                        "name": ")json" + std::string(Engine::kApiKeyParameter) + R"json(",
                        "caption": "API Key",
                        "defaultValue": ""
                    }
                ]
            }
        ]
    }
}
)json";
}

extern "C" NX_PLUGIN_API nx::sdk::IPlugin* createNxPlugin()
{
    // The object will be freed when the Server calls releaseRef().
    return new Plugin();
}

} // namespace nx::vms_server_plugins::analytics::gpt4vision
