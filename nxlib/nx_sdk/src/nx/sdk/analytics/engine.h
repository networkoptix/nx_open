#pragma once

#include <plugins/plugin_api.h>
#include <nx/sdk/common.h>

#include <nx/sdk/i_string.h>
#include <nx/sdk/i_plugin_event.h>

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
 *
 * All methods are guaranteed to be called without overlappings, even if from different threads,
 * thus, no synchronization is required for the implementation.
 */
class Engine: public nxpl::PluginInterface
{
public:
    class IHandler
    {
    public:
        virtual ~IHandler() = default;
        virtual void handlePluginEvent(IPluginEvent* event) = 0;
    };

    /** @return Parent Plugin. */
    virtual Plugin* plugin() const = 0;

    /**
     * Called before other methods. Server provides the set of settings stored in its database for
     * this Engine instance.
     *
     * @param settings Values of settings declared in the manifest. The pointer is valid only
     *     during the call. If count is 0, the pointer is null.
     */
    virtual void setSettings(const Settings* settings) = 0;

    /**
     * In addition to the settings stored in a Server database, an Engine can have some settings
     * which are stored somewhere "under the hood" of the Engine, e.g. on a device acting as an
     * Engine's backend. Such settings do not need to be explicitly marked in the Settings Model,
     * but every time the Server offfers the user to edit the values, it calls this method and
     * merges the received values with the ones in its database.
     *
     * @return Engine settings that are stored on the plugin side, or null if there are no such
     * settings.
     * TODO: #mshevchenko: Rename to pluginSideSettings(), here and in DeviceAgent.
     */
    virtual Settings* settings() const = 0;

    /**
     * Provides a JSON manifest for this Engine instance. See the example of such manifest in
     * stub_analytics_plugin. Can either be static (constant), or can be dynamically generated by
     * this Engine based on its current state, including setting values received via setSettings().
     * After creation of this Engine instance, this method is called after setSettings(), but can
     * be called again at any other moment to obtain the most actual manifest.
     *
     * @param outError Status of the operation; is set to noError before this call.
     * @return JSON string in UTF-8.
     */
    virtual const IString* manifest(Error* outError) const = 0;

    /**
     * Creates, or returns an already existing, a DeviceAgent instance intended to work with the
     * given device.
     *
     * @param deviceInfo Information about the device for which a DeviceAgent should be created.
     * @param outError Status of the operation; is set to noError before this call.
     * @return Pointer to an object that implements DeviceAgent interface, or null in case of
     *     failure.
     */
    virtual DeviceAgent* obtainDeviceAgent(const DeviceInfo* deviceInfo, Error* outError) = 0;

    /**
     * Action handler. Called when some action defined by this Engine is triggered by Server.
     *
     * @param action Provides data for the action such as metadata object for which the action has
     *     been triggered, and a means for reporting back action results to Server. This object
     *     should not be used after returning from this function.
     * @param outError Status of the operation; is set to noError before this call.
     */
    virtual void executeAction(Action* action, Error* outError) = 0;

    /**
     * @param handler Generic Engine-related events (errors, warning, info messages)
     *     might be reported via this handler.
     */
    virtual Error setHandler(IHandler* handler) = 0;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
