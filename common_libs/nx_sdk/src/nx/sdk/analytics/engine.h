#pragma once

#include <plugins/plugin_api.h>
#include <nx/sdk/common.h>

#include "device_agent.h"
#include "action.h"

namespace nx {
namespace sdk {
namespace analytics {

class Plugin; //< Forward declaration for the parent object.

/**
 * Each class that implements Engine interface should properly handle this GUID in its
 * queryInterface().
 */
static const nxpl::NX_GUID IID_Engine =
    {{0x4c,0x7b,0xf8,0xbf,0xac,0xf7,0x45,0x72,0x98,0x91,0xaa,0x7e,0xa0,0x56,0xea,0xb5}};

/**
 * Main interface for an Analytics Engine instance. The instances are created by a Mediaserver via
 * calling analytics::Plugin::createEngine() typically on Mediaserver start (or when a new Engine
 * is created by the system administrator), and destroyed (via releaseRef()) on Mediaserver
 * shutdown (or when an existing Engine is deleted by the system administrator).
 *
 * For the VMS end user, each Engine instance is perceived as an independent Analytics Engine
 * which has its own set of values of settings stored in the Mediaserver database.
 */
class Engine: public nxpl::PluginInterface
{
public:
    /** @return Parent Plugin. */
    virtual Plugin* plugin() const = 0;

    /**
     * Creates, or returns already existing, DeviceAgent for the given device. There must be only
     * one instance of the DeviceAgent created by the particular instance of Engine per device at
     * any given time. It means that if we pass DeviceInfo objects with the same UID multiple
     * times to the same Engine, then the pointer to exactly the same object must be returned.
     * Also, multiple devices must not share the same DeviceAgent.
     *
     * @param deviceInfo Information about the device for which a DeviceAgent should be created.
     * @param outError Status of the operation; is set to noError before this call.
     * @return Pointer to an object that implements DeviceAgent interface, or null in case of
     *     failure.
     */
    virtual DeviceAgent* obtainDeviceAgent(const DeviceInfo* deviceInfo, Error* outError) = 0;

    /**
     * Provides null terminated UTF-8 string containing a JSON manifest.
     * @param outError Status of the operation; is set to noError before this call.
     * @return Pointer to a C-style string which must be valid while this Engine object exists.
     */
    virtual const char* manifest(Error* outError) const = 0;

    /**
    * Tells Engine that the memory previously returned by manifest() pointed to
    * by data is no longer needed and may be disposed.
    */
    virtual void freeManifest(const char* data) = 0;

    /**
     * Called before other methods. Server provides the set of settings stored in its database for
     * this Engine instance.
     * @param settings Values of settings declared in the manifest. The pointer is valid only
     *     during the call. If count is 0, the pointer is null.
     */
    virtual void setSettings(const nxpl::Setting* settings, int count) = 0;

    /**
     * Action handler. Called when some action defined by this Engine is triggered by Server.
     * @param action Provides data for the action such as metadata object for which the action has
     *     been triggered, and a means for reporting back action results to Server. This object
     *     should not be used after returning from this function.
     * @param outError Status of the operation; is set to noError before this call.
     */
    virtual void executeAction(Action* action, Error* outError) = 0;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
