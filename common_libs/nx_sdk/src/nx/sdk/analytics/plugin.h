#pragma once

#include <cstdint>

#include <plugins/plugin_api.h>
#include <nx/sdk/common.h>

#include "engine.h"

namespace nx {
namespace sdk {
namespace analytics {

/**
 * Each class that implements analytics::Plugin interface should properly handle this GUID in its
 * queryInterface().
 */
static const nxpl::NX_GUID IID_Plugin =
    {{0x6d,0x73,0x71,0x36,0x17,0xad,0x43,0xf9,0x9f,0x80,0x7d,0x56,0x91,0x36,0x82,0x94}};

/**
 * Main interface for an analytics::Plugin instance. The only instance is created by a Mediaserver
 * in its start via calls to Plugin* createNxAnalyticsPlugin() which should be exported as extern
 * "C" by the plugin library, and is destroyed (via releaseRef()) on Mediaserver shutdown.
 */
class Plugin: public nxpl::Plugin2
{
public:

    /**
     * Provides null terminated UTF-8 string containing a JSON manifest.
     * @param outError Status of the operation; is set to noError before this call.
     * @return Pointer to a C-style string which must be valid while this Plugin object exists.
     */
    virtual const char* manifest(nx::sdk::Error* outError) const = 0;

    /**
    * Tells Plugin that the memory previously returned by manifest() pointed to
    * by data is no longer needed and may be disposed.
    */
    virtual void freeManifest(const char* data) = 0;

    /**
     * Creates a new instance of analytics::Engine.
     * @param outError Status of the operation; is set to noError before this call.
     * @return Pointer to an object that implements DeviceAgent interface, or null in case of
     * failure.
     */
    virtual Engine* createEngine(Error* outError) = 0;

    /**
     * Name of the plugin dynamic library, without "lib" prefix and without extension.
     */
    virtual const char* name() const override = 0;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
