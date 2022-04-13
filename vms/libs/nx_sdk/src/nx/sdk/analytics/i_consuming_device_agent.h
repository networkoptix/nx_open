// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/interface.h>

#include "i_data_packet.h"
#include "i_device_agent.h"

namespace nx {
namespace sdk {
namespace analytics {

class IConsumingDeviceAgent0: public Interface<IConsumingDeviceAgent0, IDeviceAgent0>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::analytics::IConsumingDeviceAgent"); }

    /** Called by pushDataPacket() */
    protected: virtual void doPushDataPacket(Result<void>* outResult, IDataPacket* dataPacket) = 0;
    /**
     * Supplies data to the Engine. Called from a worker thread.
     *
     * @param dataPacket Never null. Has a valid timestamp >= 0.
     */
    public: Result<void> pushDataPacket(IDataPacket* dataPacket)
    {
        Result<void> result;
        doPushDataPacket(&result, dataPacket);
        return result;
    }
};

class IConsumingDeviceAgent1: public Interface<IConsumingDeviceAgent1, IConsumingDeviceAgent0>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::analytics::IConsumingDeviceAgent1"); }

    /**
     * Called by the Server when this DeviceAgent is no longer needed, before releasing the ref.
     */
    virtual void finalize() = 0;
};

/**
 * Interface for a DeviceAgent that requires input (e.g. audio or video stream) from the Device.
 */
class IConsumingDeviceAgent: public Interface<IConsumingDeviceAgent, IConsumingDeviceAgent1>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::analytics::IConsumingDeviceAgent2"); }

    /** Called by getSettingsOnActiveSettingChange() */
    protected: virtual void doGetSettingsOnActiveSettingChange(
        Result<const ISettingsResponse*>* outResult,
        const IString* activeSettingId,
        const IString* settingsModel,
        const IStringMap* settingsValues) = 0;
    /**
     * When a setting marked as Active changes its value in the GUI, the Server calls this method
     * to notify the Plugin and allow it to adjust the values of the settings and the Settings
     * Model. This mechanism allows certain settings to depend on the current values of others,
     * for example, switching a checkbox or a drop-down can lead to some other setting being
     * replaced with another, or some values being converted to a different measurement unit.
     *
     * @param activeSettingId Id of a setting which has triggered this notification.
     *
     * @param settingsModel Model of settings the Active setting has been triggered on. Never null.
     *
     * @param settingsValues Values currently set in the GUI. Never null.
     *
     * @return An error code with a message in case of some general failure that affected the
     *     procedure of analyzing the settings, or a combination of optional individual setting
     *     errors, optional new setting values in case they were adjusted, and an optional new
     *     Settings Model. Can be null if none of the above items are present.
     */
    public: Result<const ISettingsResponse*> getSettingsOnActiveSettingChange(
        const IString* activeSettingId,
        const IString* settingsModel,
        const IStringMap* settingsValues)
    {
        Result<const ISettingsResponse*> result;
        doGetSettingsOnActiveSettingChange(&result, activeSettingId, settingsModel, settingsValues);
        return result;
    }
};
using IConsumingDeviceAgent2 = IConsumingDeviceAgent;

} // namespace analytics
} // namespace sdk
} // namespace nx
