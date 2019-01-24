#pragma once

#include <plugins/plugin_api.h>

namespace nx {
namespace sdk {

/**
 * Each class that implements IDeviceInfo interface should properly handle this GUID in
 * its queryInterface method.
 */
static const nxpl::NX_GUID IID_DeviceInfo =
    {{0xf5,0x67,0x64,0xc3,0x01,0x3c,0x49,0xc8,0xb6,0xf9,0x82,0xcf,0xaf,0xbe,0xbc,0x33}};

/**
 * Provides various information about a Device. For multichannel devices, the Device represents a
 * single channel.
 */
class IDeviceInfo: public nxpl::PluginInterface
{
public:
    /* @return Unique id of the device. */
    virtual const char* id() const = 0;

    /* @return Human-readable name of the device vendor, in UTF-8. */
    virtual const char* vendor() const = 0;

    /* @return Model of the device, in UTF-8. */
    virtual const char* model() const = 0;

    /* @return Version of the firmware installed on the device, in UTF-8. */
    virtual const char* firmware() const = 0;

    /* @return Human-readable name of the device assigned by a VMS user, in UTF-8. */
    virtual const char* name() const = 0;

    /* @return URL of the device. */
    virtual const char* url() const = 0;

    /* @return Login of the device. */
    virtual const char* login() const = 0;

    /* @return Password of the device, to be used with login(). */
    virtual const char* password() const = 0;

    /**
     * @return Id of the group of the devices this device belongs to. For example, all the channels
     *     of an NVR or multichannel encoder have the same sharedId.
     */
    virtual const char* sharedId() const = 0;

    /**
     * @return Id of the device assigned by a VMS user. Used for integrations with third-party
     *     systems.
     */
    virtual const char* logicalId() const = 0;

    /**
     * @return Zero-based index of a channel represented by the device.
     */
    virtual int channelNumber() const = 0;
};

} // namespace sdk
} // namespace nx
