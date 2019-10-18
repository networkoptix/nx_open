// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "engine.h"

#include "device_agent.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace sample {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

Engine::Engine():
    // Call the DeviceAgent helper class constructor telling it to verbosely report to stderr.
    nx::sdk::analytics::Engine(/*enableOutput*/ true)
{
}

Engine::~Engine()
{
}

/**
 * Called when the Server opens a video-connection to the camera if the plugin is enabled for this
 * camera.
 *
 * @param outResult The pointer to the structure which needs to be filled with the resulting value
 *     or the error information.
 * @param deviceInfo Contains various information about the related device such as its id, vendor,
 *     model, etc.
 */
void Engine::doObtainDeviceAgent(Result<IDeviceAgent*>* outResult, const IDeviceInfo* deviceInfo)
{
    *outResult = new DeviceAgent(deviceInfo);
}

/**
 *  @return JSON with the particular structure. Note that it is possible to fill in the values
 * that are not known at compile time, but should not depend on the Engine settings.
 */
std::string Engine::manifestString() const
{
    // Ask the Server to supply uncompressed video frames in YUV420 format (see
    // https://en.wikipedia.org/wiki/YUV).
    //
    // Note that this format is used internally by the Server, therefore requires minimum
    // resources for decoding, thus it is the recommended format.
    return /*suppress newline*/ 1 + R"json(
{
    "capabilities": "needUncompressedVideoFrames_yuv420"
}
)json";
}

} // namespace sample
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx

/** Plugin manifest is formatted as JSON with the particular structure.
 *
 * - id: Unique identifier for a plugin with format "{vendor_id}.{plugin_id}", where
 *     {vendor_id} is the unique identifier of the plugin creator (person or company name) and
 *     {plugin_id} is the unique (for a specific vendor) identifier of the plugin.
 * - name: A human-readable short name of the plugin (displayed in the "Camera Settings" window
 *     of the Client).
 * - description: Description of the plugin in a few sentences.
 * - version: Version of the plugin.
 * - vendor: Plugin creator (person or company) name.
 */
static const std::string kPluginManifest = /*suppress newline*/ 1 + R"json(
{
    "id": "nx.sample",
    "name": "Sample analytics plugin",
    "description": "A simple \"Hello, world!\" plugin that can be used as a template.",
    "version": "1.0.0",
    "vendor": "Plugin vendor"
})json";

/**
 * Called by the Server to instantiate the Plugin object.
 *
 * The Server requires the function to have C linkage, which leads to no C++ name mangling in the
 * export table of the plugin dynamic library, so that makes it possible to write plugins in any
 * language and compiler.
 *
 * NX_PLUGIN_API is the macro defined by CMake scripts for exporting the function.
 */
extern "C" NX_PLUGIN_API nx::sdk::IPlugin* createNxPlugin()
{
    // The object will be freed when the Server calls releaseRef().
    return new nx::sdk::analytics::Plugin(
        /*pluginManifest*/ kPluginManifest,
        // The lambda will be called when the Server decides to create the Engine instance.
        /*createEngine*/ [](nx::sdk::analytics::Plugin* /*plugin*/)
        {
            // The object will be freed when the Server calls releaseRef().
            return new nx::vms_server_plugins::analytics::sample::Engine();
        });
}
