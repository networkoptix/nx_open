#pragma once

#include <nx/sdk/interface.h>

namespace nx {
namespace sdk {

/**
 * Provides various information about a Device. For multichannel devices, the Device represents a
 * single channel.
 */
class IDeviceInfo: public nx::sdk::Interface<IDeviceInfo>
{
public:
    static auto interfaceId() { return InterfaceId("nx::sdk::IDeviceInfo"); }

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
